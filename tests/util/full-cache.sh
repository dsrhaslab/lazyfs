#!/usr/bin/env bash
# Writes past a full cache without eviction go through to the backing file.
# Check their contents, before and after cache loss, and, with strace, that
# each request reaches the backing file as one write per run of blocks
# rather than one per block.
set -euo pipefail
binary=$(realpath "${1:?Pass the LazyFS executable}")
root=$(mktemp -d "${TMPDIR:-/tmp}/lazyfs-full-cache.XXXXXXXX")
mounted="$root/mounted"
mkdir -p "$mounted" "$root/backing"
fs_pid=''
tracer=''
cleanup() {
    if [[ -n $tracer ]]; then
        kill "$tracer" 2>/dev/null || true
        wait "$tracer" 2>/dev/null || true
    fi
    if mountpoint -q "$mounted"; then
        fusermount3 -u "$mounted" || true
    fi
    if [[ -n $fs_pid ]]; then
        kill -KILL "$fs_pid" 2>/dev/null || true
        wait "$fs_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT
cat > "$root/config.toml" <<EOF
[faults]
fifo_path="$root/commands.fifo"
fifo_path_completed="$root/completed.fifo"
[cache]
apply_eviction=false
[cache.simple]
custom_size="1mb"
blocks_per_page=1
[filesystem]
logfile="$root/lazyfs.log"
log_all_operations=false
EOF
"$binary" "$mounted" --root-dir "$root/backing" \
    --config-path "$root/config.toml" -f > "$root/mount.log" 2>&1 &
fs_pid=$!
deadline=$((SECONDS+15))
until mountpoint -q "$mounted"; do
    kill -0 "$fs_pid"
    (( SECONDS < deadline )) || exit 1
    sleep 0.02
done

# Fill the cache, then write through it.
head -c 2M /dev/urandom > "$root/fill"
dd if="$root/fill" of="$mounted/fill" bs=350k oflag=dsync status=none
head -c 8M /dev/urandom > "$root/expected"
if command -v strace > /dev/null; then
    strace -f -qq -e trace=pwrite64 -o "$root/pwrite.trace" -p "$fs_pid" &
    tracer=$!
    sleep 0.5
fi
started=$(date +%s%N)
dd if="$root/expected" of="$mounted/data" bs=350k oflag=dsync status=none
elapsed=$(( ($(date +%s%N) - started) / 1000000 ))
if [[ -n $tracer ]]; then
    kill "$tracer"
    wait "$tracer" 2>/dev/null || true
    tracer=''
    requests=$(( (8 * 1024 + 349) / 350 ))
    writes=$(grep -c 'pwrite64(' "$root/pwrite.trace" || true)
    if (( writes > 2 * requests )); then
        echo "Full-cache writes were not coalesced: $writes backing writes for $requests requests" >&2
        exit 1
    fi
    printf 'Backing writes: %d for %d requests\n' "$writes" "$requests"
fi
printf 'Wrote 8 MiB past a full cache in %d ms\n' "$elapsed"

# An unaligned overwrite spanning blocks.
head -c 100000 /dev/urandom > "$root/patch"
dd if="$root/patch" of="$mounted/data" bs=100000 count=1 seek=1234567 \
    oflag=seek_bytes,dsync conv=notrunc status=none
dd if="$root/patch" of="$root/expected" bs=100000 count=1 seek=1234567 \
    oflag=seek_bytes conv=notrunc status=none

check() {
    cmp -s "$mounted/data" "$root/expected" || { echo "Data differs $1" >&2; exit 1; }
    cmp -s "$mounted/fill" "$root/fill" || { echo "Fill differs $1" >&2; exit 1; }
}
check 'before cache loss'
# shellcheck disable=SC2016
timeout 10 bash -c 'read -r answer < "$1"; test "$answer" = finished::clear-cache' \
    _ "$root/completed.fifo" &
reader=$!
# shellcheck disable=SC2016
timeout 5 bash -c 'echo lazyfs::clear-cache > "$1"' _ "$root/commands.fifo"
wait "$reader"
check 'after cache loss'
printf 'Full-cache write regressions passed; artifacts: %s\n' "$root"
