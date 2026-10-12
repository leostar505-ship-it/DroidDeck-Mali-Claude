#!/usr/bin/bash
set -euxo pipefail
MESA_VERSION=${MESA_VERSION:-26.2.4}
WORK=/work
cd "$WORK"
grep -q '^DisableSandbox' /etc/pacman.conf || sed -i 's/^\[options\]/[options]\nDisableSandbox/' /etc/pacman.conf
{ for m in https://ca.us.mirror.archlinuxarm.org https://fl.us.mirror.archlinuxarm.org https://de3.mirror.archlinuxarm.org https://nl.mirror.archlinuxarm.org; do echo "Server = $m/\$arch/\$repo"; done; cat /etc/pacman.d/mirrorlist; } > /etc/pacman.d/mirrorlist.new
mv /etc/pacman.d/mirrorlist.new /etc/pacman.d/mirrorlist
pacman -Syu --noconfirm --needed base-devel meson ninja python python-mako python-yaml python-setuptools python-packaging pkgconf glslang spirv-tools libdrm libxcb libx11 libxext libxfixes libxshmfence libxrandr libxxf86vm xcb-util-keysyms wayland wayland-protocols libdisplay-info libelf expat zlib zstd llvm llvm-libs clang libxml2 libffi patchelf zip curl tar xz bison flex

rm -rf build-mesa && mkdir build-mesa && cd build-mesa
curl -fsSL -o mesa.tar.xz "https://archive.mesa3d.org/mesa-${MESA_VERSION}.tar.xz"
tar -xJf mesa.tar.xz
python3 "$WORK/tools/lavapipe/patch-mesa.py" "mesa-${MESA_VERSION}"
grep -rn "dma_heap_fd" "mesa-${MESA_VERSION}/src/gallium/drivers/llvmpipe" | head

meson setup build "mesa-${MESA_VERSION}" \
  -Dbuildtype=release \
  -Db_ndebug=true \
  -Dplatforms=x11,wayland \
  -Dgallium-drivers=llvmpipe \
  -Dvulkan-drivers=swrast \
  -Dllvm=enabled \
  -Dshared-llvm=enabled \
  -Dglx=disabled \
  -Degl=disabled \
  -Dgbm=disabled \
  -Dopengl=false \
  -Dvalgrind=disabled \
  -Dlibunwind=disabled \
  -Dlmsensors=disabled
ninja -C build

lib=$(find build -name libvulkan_lvp.so | head -1)
test -n "$lib"
cd "$WORK"
rm -rf out && mkdir -p out/pkg
cp "build-mesa/$lib" out/pkg/libvulkan_lvp.so
llvm_lib=$(readelf -d out/pkg/libvulkan_lvp.so | sed -n 's/.*NEEDED.*\[\(libLLVM\.so\.[0-9.]*\)\].*/\1/p' | head -1)
test -n "$llvm_lib"
cp "$(readlink -f /usr/lib/libLLVM.so)" "out/pkg/$llvm_lib"
patchelf --set-rpath '$ORIGIN' out/pkg/libvulkan_lvp.so
readelf -d out/pkg/libvulkan_lvp.so | grep -E "NEEDED|RUNPATH|SONAME"
printf '{\n  "schemaVersion": 1,\n  "kind": "linux-vulkan-icd",\n  "name": "Lavapipe %s dma-heap (CPU)",\n  "driverVersion": "%s",\n  "libc": "glibc",\n  "minGlibc": "2.43"\n}\n' "$MESA_VERSION" "$MESA_VERSION" > out/pkg/meta.json
(cd out/pkg && zip -9 "../Lavapipe-${MESA_VERSION}-dmaheap-Linux.zip" meta.json libvulkan_lvp.so "$llvm_lib")
echo "$MESA_VERSION" > out/version
ls -la out
