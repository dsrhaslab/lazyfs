#!/usr/bin/env bash
set -euo pipefail
binary=$(realpath "${1:?Pass the LazyFS executable}")
root=$(mktemp -d "${TMPDIR:-/tmp}/lazyfs-shutdown.XXXXXXXX")
fs_pid=''
mounted=''
cleanup() {
    if [[ -n $mounted ]] && mountpoint -q "$mounted"; then
        fusermount3 -u "$mounted" || true
    fi
    if [[ -n $fs_pid ]]; then
        kill -KILL "$fs_pid" 2>/dev/null || true
        wait "$fs_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT
for mode in plain idle-completion active-completion blocked-completion; do
    directory="$root/$mode"
    mkdir -p "$directory/mounted" "$directory/backing"
    mounted="$directory/mounted"
    cat > "$directory/config.toml" <<EOF
[faults]
fifo_path="$directory/commands.fifo"
EOF
    if [[ $mode != plain ]]; then
        printf 'fifo_path_completed="%s/completed.fifo"\n' "$directory" >> "$directory/config.toml"
    fi
    cat >> "$directory/config.toml" <<EOF
[cache]
apply_eviction=false
[cache.simple]
custom_size="1mb"
blocks_per_page=1
[filesystem]
logfile="$directory/lazyfs.log"
log_all_operations=false
EOF
    "$binary" "$mounted" --root-dir "$directory/backing" \
        --config-path "$directory/config.toml" -f > "$directory/mount.log" 2>&1 &
    fs_pid=$!
    deadline=$((SECONDS+15))
    until mountpoint -q "$mounted"; do
        kill -0 "$fs_pid"
        (( SECONDS < deadline )) || exit 1
        sleep 0.02
    done
    if [[ $mode == active-completion ]]; then
        for _ in 1 2; do
            # Each reader starts before its command, as in the Jepsen adapter.
            # shellcheck disable=SC2016
            timeout 10 bash -c 'read -r answer < "$1"; test "$answer" = finished::clear-cache' \
                _ "$directory/completed.fifo" &
            reader=$!
            # shellcheck disable=SC2016
            timeout 5 bash -c 'echo lazyfs::clear-cache > "$1"' _ "$directory/commands.fifo"
            wait "$reader"
        done
    elif [[ $mode == blocked-completion ]]; then
        # shellcheck disable=SC2016
        timeout 5 bash -c 'echo lazyfs::clear-cache > "$1"' _ "$directory/commands.fifo"
        deadline=$((SECONDS+5))
        until grep -q 'awaiting clear-cache completion listener' "$directory/lazyfs.log"; do
            (( SECONDS < deadline )) || exit 1
            sleep 0.02
        done
    fi
    fusermount3 -u "$mounted"
    deadline=$((SECONDS+5))
    while kill -0 "$fs_pid" 2>/dev/null; do
        (( SECONDS < deadline )) || { echo "Fault worker did not stop: $mode" >&2; exit 1; }
        sleep 0.02
    done
    wait "$fs_pid"
    fs_pid=''
    printf 'Clean unmount: %s\n' "$mode"
done
printf 'Shutdown regressions passed; artifacts: %s\n' "$root"
