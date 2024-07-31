find -regex '.*/.*\.\(c\|cpp\|hpp\)$' | \
sed 's/.\/include/$\{PROJECT_SOURCE_DIR\}\/include/g' | \
awk '!/^(.\/build)/' | \
sed 's/\.\///g'
