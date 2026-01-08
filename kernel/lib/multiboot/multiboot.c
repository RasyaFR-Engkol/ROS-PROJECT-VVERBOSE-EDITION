#include <multiboot.h>

// Helper
static inline size_t align_up(size_t value, size_t align) {
    return (value + (align - 1)) & ~(align - 1);
}

namespace KernelIntuitive{
    namespace Multiboot{
        VOID ParseMultiboot2Info(uint64_t mb_info_ptr) {
            BootInfo info = {};
            info.multiboot_addr = mb_info_ptr;

            if (mb_info_ptr == 0) {
                gBootInfo = info;
                return;
            }

            const uint8_t *base = (const uint8_t *)(uintptr_t)mb_info_ptr;
            if (!base) {
                gBootInfo = info;
                return;
            }

            /* Read the 32-bit little-endian total size from the byte buffer without
                performing an aligned pointer cast. This avoids -Wcast-align and is
                safe on architectures that disallow unaligned accesses. */
            uint32_t total_size = (uint32_t)base[0]
                | ((uint32_t)base[1] << 8)
                | ((uint32_t)base[2] << 16)
                | ((uint32_t)base[3] << 24);
            info.multiboot_total_size = total_size;
            if (total_size < 8U) {
                gBootInfo = info;
                return;
            }

            const uint8_t *tag_ptr = base + 8U;
            const uint8_t *end = base + total_size;

            while ((tag_ptr + sizeof(struct multiboot_tag)) <= end) {
                const struct multiboot_tag *tag = (const struct multiboot_tag *)tag_ptr;

                if (info.tag_trace.count < 32) {
                    info.tag_trace.types[info.tag_trace.count++] = tag->type;
                }

                if (tag->type == MB_TAG_TYPE_END) {
                    break;
                }

                if (tag->size < sizeof(struct multiboot_tag)) {
                    break;
                }

                if (tag->type == MB_TAG_TYPE_FRAMEBUFFER &&
                    tag->size >= sizeof(struct multiboot_tag_framebuffer)) {
                    const struct multiboot_tag_framebuffer *fb =
                        (const struct multiboot_tag_framebuffer *)tag;

                    info.has_framebuffer = TRUE;
                    info.framebuffer.address = fb->framebuffer_addr;
                    info.framebuffer.pitch = fb->framebuffer_pitch;
                    info.framebuffer.width = fb->framebuffer_width;
                    info.framebuffer.height = fb->framebuffer_height;
                    info.framebuffer.bpp = fb->framebuffer_bpp;
                    info.framebuffer.type = fb->framebuffer_type;
                    info.framebuffer.red_position = 0;
                    info.framebuffer.red_mask_size = 0;
                    info.framebuffer.green_position = 0;
                    info.framebuffer.green_mask_size = 0;
                    info.framebuffer.blue_position = 0;
                    info.framebuffer.blue_mask_size = 0;

                    if (fb->framebuffer_type == 1U) {
                        info.framebuffer.red_position = fb->color_info.rgb.framebuffer_red_field_position;
                        info.framebuffer.red_mask_size = fb->color_info.rgb.framebuffer_red_mask_size;
                        info.framebuffer.green_position = fb->color_info.rgb.framebuffer_green_field_position;
                        info.framebuffer.green_mask_size = fb->color_info.rgb.framebuffer_green_mask_size;
                        info.framebuffer.blue_position = fb->color_info.rgb.framebuffer_blue_field_position;
                        info.framebuffer.blue_mask_size = fb->color_info.rgb.framebuffer_blue_mask_size;
                    }
                } else if (tag->type == MB_TAG_TYPE_MMAP && tag->size >= sizeof(struct multiboot_tag_mmap)) {
                    const struct multiboot_tag_mmap *mm = (const struct multiboot_tag_mmap *)tag;
                    info.memmap.entry_size = mm->entry_size;
                    info.memmap.entry_version = mm->entry_version;
                    info.memmap.count = 0;

                    const uint8_t *ptr = tag_ptr + sizeof(struct multiboot_tag_mmap);
                    const uint8_t *tend = tag_ptr + tag->size;
                    while (ptr + mm->entry_size <= tend && info.memmap.count < 128) {
                        const struct multiboot_mmap_entry *e = (const struct multiboot_mmap_entry *)ptr;
                        BootMemRegion *dst = &info.memmap.regions[info.memmap.count++];
                        dst->base = e->addr;
                        dst->length = e->len;
                        dst->type = e->type;
                        dst->reserved = 0;
                        ptr += mm->entry_size;
                    }
                    if (info.memmap.count > 0) info.has_memmap = TRUE;
                } else if ((tag->type == MB_TAG_TYPE_ACPI_OLD || tag->type == MB_TAG_TYPE_ACPI_NEW) &&
                        tag->size >= sizeof(struct multiboot_tag)) {
                    const uint8_t *rsdp = tag_ptr + sizeof(struct multiboot_tag);
                    uint32_t rsdp_len = tag->size - sizeof(struct multiboot_tag);
                    if (rsdp_len >= 20U) {
                        info.has_acpi = TRUE;
                        info.acpi.rsdp = (const void *)rsdp;
                        info.acpi.length = rsdp_len;
                        info.acpi.revision = rsdp[15];
                        info.acpi.is_xsdp = (tag->type == MB_TAG_TYPE_ACPI_NEW) ? 1U : 0U;
                    }
                }

                size_t advance = align_up(tag->size, MB_ALIGN);
                if (advance == 0U || tag_ptr + advance > end) {
                    break;
                }

                tag_ptr += advance;
            }

            gBootInfo = info;
        }

        ABI_C const BootInfo *BootInfoGet(void) {
            return &gBootInfo;
        }
    }
}