import sys
from pathlib import Path

root = Path(sys.argv[1])


def edit(relative, old, new):
    path = root / relative
    text = path.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"{relative}: expected exactly one match, found {text.count(old)} for:\n{old}")
    path.write_text(text.replace(old, new, 1))


edit(
    "src/gallium/drivers/llvmpipe/lp_screen.h",
    "   int udmabuf_fd;\n",
    "   int udmabuf_fd;\n   int dma_heap_fd;\n",
)

edit(
    "src/gallium/drivers/llvmpipe/lp_screen.c",
    "   else if (lscreen->udmabuf_fd != -1)\n      caps->dmabuf = DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT;",
    "   else if (lscreen->udmabuf_fd != -1 || lscreen->dma_heap_fd != -1)\n      caps->dmabuf = DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT;",
)

edit(
    "src/gallium/drivers/llvmpipe/lp_screen.c",
    '#include "lp_screen.h"\n',
    '#include <errno.h>\n#include <stdio.h>\n#include "lp_screen.h"\n',
)

edit(
    "src/gallium/drivers/llvmpipe/lp_screen.c",
    '   screen->udmabuf_fd = open("/dev/udmabuf", O_RDWR);\n',
    '   screen->udmabuf_fd = open("/dev/udmabuf", O_RDWR);\n'
    '   screen->dma_heap_fd = open("/dev/dma_heap/system", O_RDONLY | O_CLOEXEC);\n'
    '   fprintf(stderr, "lvp-dmaheap: open fd=%d errno=%d udmabuf=%d\\n", screen->dma_heap_fd, screen->dma_heap_fd < 0 ? errno : 0, screen->udmabuf_fd);\n',
)

edit(
    "src/gallium/drivers/llvmpipe/lp_screen.c",
    "   if (screen->udmabuf_fd != -1)\n      close(screen->udmabuf_fd);\n",
    "   if (screen->udmabuf_fd != -1)\n      close(screen->udmabuf_fd);\n"
    "   if (screen->dma_heap_fd != -1)\n      close(screen->dma_heap_fd);\n",
)

edit(
    "src/gallium/drivers/llvmpipe/lp_texture.c",
    "#include <linux/udmabuf.h>\n",
    "#include <linux/udmabuf.h>\n#include <linux/dma-heap.h>\n#include <errno.h>\n#include <stdio.h>\n",
)

edit(
    "src/gallium/drivers/llvmpipe/lp_texture.c",
    "   if (screen->udmabuf_fd < 0)\n      return false;\n",
    """   if (screen->udmabuf_fd < 0) {
      if (screen->dma_heap_fd < 0) {
         fprintf(stderr, "lvp-dmaheap: no heap fd\\n");
         return false;
      }

      uint64_t heap_alignment;
      if (!os_get_page_size(&heap_alignment))
         heap_alignment = 4096;

      struct dma_heap_allocation_data heap_alloc = {
         .len = align(size, heap_alignment),
         .fd_flags = O_RDWR | O_CLOEXEC,
      };

      if (ioctl(screen->dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_alloc) < 0) {
         fprintf(stderr, "lvp-dmaheap: ALLOC len=%llu failed errno=%d\\n", (unsigned long long)heap_alloc.len, errno);
         return false;
      }

      void *heap_addr = mmap(NULL, heap_alloc.len, PROT_WRITE | PROT_READ,
                             MAP_SHARED, heap_alloc.fd, 0);
      if (heap_addr == MAP_FAILED) {
         fprintf(stderr, "lvp-dmaheap: mmap failed errno=%d\\n", errno);
         close(heap_alloc.fd);
         return false;
      }

      *alloc = (struct llvmpipe_memory_allocation){
         .fd = heap_alloc.fd,
         .offset = 0,
         .cpu_addr = heap_addr,
         .size = heap_alloc.len,
         .type = LLVMPIPE_MEMORY_FD_TYPE_DMA_BUF,
         .mem_fd = -1,
      };

      return true;
   }
""",
)

print("patched")
