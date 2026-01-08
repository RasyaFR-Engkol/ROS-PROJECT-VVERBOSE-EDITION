#pragma once

#include <bootinfo.h>

#define MB_TAG_TYPE_END 0U
#define MB_TAG_TYPE_FRAMEBUFFER 8U
#define MB_TAG_TYPE_MMAP 6U
#define MB_TAG_TYPE_ACPI_OLD 14U
#define MB_TAG_TYPE_ACPI_NEW 15U
#define MB_ALIGN 8U

namespace KernelIntuitive{
    namespace Multiboot{
        static BootInfo gBootInfo;

        struct ROS_PACKED multiboot_tag {
            U32 type;
            U32 size;
        };

        struct ROS_PACKED multiboot_tag_framebuffer {
            U32 type;
            U32 size;
            U64 framebuffer_addr;
            U32 framebuffer_pitch;
            U32 framebuffer_width;
            U32 framebuffer_height;
            U8 framebuffer_bpp;
            U8 framebuffer_type;
            U16 reserved;
            union {
                struct {
                    U16 framebuffer_palette_num_colors;
                    /* Optional palette entries follow */
                } palette;
                struct {
                    U8 framebuffer_red_field_position;
                    U8 framebuffer_red_mask_size;
                    U8 framebuffer_green_field_position;
                    U8 framebuffer_green_mask_size;
                    U8 framebuffer_blue_field_position;
                    U8 framebuffer_blue_mask_size;
                } rgb;
            } color_info;
        };

        struct ROS_PACKED multiboot_tag_mmap {
            U32 type;
            U32 size;
            U32 entry_size;
            U32 entry_version;
        };

        struct ROS_PACKED multiboot_mmap_entry {
            U64 addr;
            U64 len;
            U32 type;
            U32 zero;
        };

        VOID ParseMultiboot2Info(uint64_t mb_info_ptr);
    }
}

