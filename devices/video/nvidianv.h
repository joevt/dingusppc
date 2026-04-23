/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
Ported from Bochs bx_geforce_c (Copyright 2025-2026 The Bochs Project, LGPL v2+).
*/

/**
 * @file  nvidianv.h
 * @brief NVIDIA NV-architecture GeForce for DingusPPC.
 *
 * BAR layout:
 *   BAR0  16 MB   NV register space (PMC/PBUS/PFIFO/PGRAPH/PCRTC/PRAMDAC)
 *   BAR1  64-256 MB  VRAM (prefetchable)
 *   BAR2  512 KB / 16 MB  RAMIN aperture (NV20/NV40 only)
 *   ROM   FCode + Mac OS X driver stub
 */

#ifndef NVIDIA_NV_H
#define NVIDIA_NV_H

#include <devices/common/pci/pcidevice.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <chrono>
#include <memory>

// ---- Constants ----
#define NV_MMIO_SIZE        0x1000000u
#define NV_CHANNEL_COUNT    32
#define NV_SUBCHANNEL_COUNT 8
#define NV_CACHE1_SIZE      64
#define NV_CRTC_EXT_MAX     0xF0

enum NvCardType : uint32_t {
    NV_TYPE_NV15 = 0x15,
    NV_TYPE_NV20 = 0x20,
    NV_TYPE_NV35 = 0x35,
    NV_TYPE_NV40 = 0x40,
};

// ---- Texture descriptor ----
struct NvTexture {
    uint32_t offset, dma_obj, format;
    bool cubemap, linear, unnormalized, compressed;
    bool dxt_alpha_data, dxt_alpha_explicit;
    uint32_t color_bytes, levels;
    uint32_t base_size[3], size[3];
    uint32_t face_bytes, wrap[3];
    uint32_t control0;
    bool enabled;
    uint32_t control1;
    bool signed_any, signed_comp[4];
    uint32_t image_rect, pal_dma_obj, pal_ofs, control3, key_color;
    float offset_matrix[4];
};

// ---- Per-FIFO-channel state ----
struct NvChannel {
    uint32_t subr_return;
    bool     subr_active;
    struct { uint32_t mthd, subc, mcnt; bool ni; } dma_state;
    struct { uint32_t object; uint8_t engine; uint32_t notifier; } schs[NV_SUBCHANNEL_COUNT];
    bool notify_pending; uint32_t notify_type;

    // 2D surface/blit
    bool s2d_locked;
    uint32_t s2d_img_src, s2d_img_dst, s2d_color_fmt, s2d_color_bytes;
    uint32_t s2d_pitch_src, s2d_pitch_dst;
    uint32_t s2d_ofs_src, s2d_ofs_dst;
    bool     blit_color_key_enable;
    uint32_t blit_operation, blit_syx, blit_dyx, blit_hw;
    uint32_t swzs_color_bytes, swzs_img_dst, swzs_color_fmt, swzs_base, swzs_size;
    uint32_t swzs_pitch, swzs_base_src, swzs_x, swzs_y, swzs_width, swzs_height;
    uint32_t m2mf_obj_src, m2mf_obj_dst, m2mf_ofs_src, m2mf_ofs_dst;
    uint32_t m2mf_pitch_src, m2mf_pitch_dst, m2mf_line_len, m2mf_line_count, m2mf_format;
    uint32_t surf3d_obj_z, surf3d_obj_color, surf3d_pitch_z, surf3d_pitch_color;
    uint32_t ifc_color_fmt, ifc_color_bytes, ifc_width, ifc_height;
    uint32_t ifc_x, ifc_y, ifc_pitch, ifc_words_left;
    uint32_t sifc_color_fmt, sifc_color_bytes, sifc_width, sifc_height;
    uint32_t sifc_x, sifc_y, sifc_dsdx, sifc_dtdy, sifc_u, sifc_v;
    uint32_t sifc_srcw, sifc_srch, sifc_src_obj, sifc_src_ofs, sifc_src_pitch;
    uint32_t iifc_color_fmt_src, iifc_color_fmt_dst, iifc_color_bytes;
    uint32_t iifc_src_obj, iifc_src_ofs, iifc_src_pitch, iifc_dst_ofs, iifc_dst_pitch;
    uint32_t iifc_dsdx, iifc_dtdy, iifc_u, iifc_v, iifc_x, iifc_y;
    uint32_t iifc_width, iifc_height, iifc_srcw, iifc_srch;
    uint32_t tfc_src_obj, tfc_src_ofs, tfc_src_pitch, tfc_x, tfc_y, tfc_width, tfc_height;
    uint32_t sifm_src_obj, sifm_src_ofs, sifm_src_pitch, sifm_src_size;
    uint32_t sifm_x, sifm_y, sifm_width, sifm_height;
    uint32_t sifm_du, sifm_dv, sifm_u, sifm_v, sifm_color_fmt;
    uint32_t imageblit_height, imageblit_x, imageblit_y, imageblit_width, imageblit_words_left;

    // 3D / D3D
    uint32_t d3d_color_bytes, d3d_depth_bytes, d3d_color_fmt, d3d_depth_fmt;
    uint32_t d3d_color_ofs, d3d_depth_ofs, d3d_pitch_color, d3d_pitch_depth;
    uint32_t d3d_scissor_x, d3d_scissor_y, d3d_scissor_width, d3d_scissor_height;
    uint32_t d3d_viewport_x, d3d_viewport_y, d3d_viewport_width, d3d_viewport_height;
    float d3d_viewport_offset[4];
    float d3d_combiner_const_color[8][2][4];
    uint32_t d3d_combiner_alpha_ocw[8], d3d_combiner_color_icw[8];
    float d3d_viewport_scale[4];
    uint32_t d3d_transform_program[544][4];
    float d3d_transform_constant[512][4];
    float d3d_light_ambient_color[8][3], d3d_light_diffuse_color[8][3];
    float d3d_light_specular_color[8][3], d3d_light_inf_half_vector[8][3];
    float d3d_light_inf_direction[8][3];
    float d3d_normal[3], d3d_diffuse_color[4], d3d_texcoord[4][4];
    uint32_t d3d_attrib_count, d3d_vertex_data_base_index;
    uint32_t d3d_vertex_data_array_offset[16];
    uint32_t d3d_vertex_data_array_format_type[16];
    uint32_t d3d_vertex_data_array_format_size[16];
    uint32_t d3d_vertex_data_array_format_stride[16];
    bool d3d_vertex_data_array_format_dx[16];
    bool d3d_vertex_data_array_format_homogeneous[16];
    uint32_t d3d_begin_end;
    bool d3d_primitive_done, d3d_triangle_flip;
    uint32_t d3d_vertex_index, d3d_attrib_index, d3d_comp_index;
    float d3d_vertex_data[4][16][4];
    uint32_t d3d_index_array_offset, d3d_index_array_dma;
    NvTexture d3d_texture[16];
    uint32_t d3d_semaphore_obj, d3d_semaphore_offset;
    uint32_t d3d_zstencil_clear_value, d3d_color_clear_value, d3d_clear_surface;
    uint32_t d3d_combiner_color_ocw[8];
    uint32_t d3d_combiner_control, d3d_combiner_control_num_stages;
    uint32_t d3d_tex_shader_op[4], d3d_tex_shader_previous[4];
    uint32_t d3d_transform_execution_mode, d3d_transform_program_load;
    uint32_t d3d_transform_program_start, d3d_transform_constant_load;
    uint32_t d3d_attrib_in_normal, d3d_attrib_in_color[2], d3d_attrib_out_color[2];
    uint32_t d3d_attrib_in_tex_coord[16], d3d_attrib_out_tex_coord[16];
    bool d3d_attrib_out_enable[32];
    uint32_t d3d_vs_temp_regs_count, d3d_tex_coord_count;

    // 2D object
    uint8_t rop; uint32_t beta;
    uint16_t clip_x, clip_y, clip_width, clip_height;
    uint32_t chroma_color_fmt, chroma_color;
    uint32_t patt_shape; bool patt_type_color;
    uint32_t patt_bg_color, patt_fg_color;
    bool patt_data_mono[64]; uint32_t patt_data_color[64];
    uint32_t gdi_operation, gdi_color_fmt, gdi_mono_fmt;
    uint32_t gdi_clip_yx0, gdi_clip_yx1, gdi_rect_color, gdi_rect_xy;
    uint32_t gdi_rect_yx0, gdi_rect_yx1, gdi_rect_wh;
    uint32_t gdi_bg_color, gdi_fg_color;
    uint32_t gdi_image_swh, gdi_image_dwh, gdi_image_xy;
    uint32_t gdi_words_ptr, gdi_words_left; uint32_t *gdi_words;
    uint32_t rect_operation, rect_color_fmt, rect_color, rect_yx, rect_hw;
};

// ---- Main device class ----

class NvidiaNV : public PCIDevice, public VideoCtrlBase {
public:
    NvidiaNV() : NvidiaNV(NV_TYPE_NV20) {}  // default: GeForce3
    ~NvidiaNV() = default;

    static std::unique_ptr<HWComponent> create_nv15(const std::string &) { return std::make_unique<NvidiaNV>(NV_TYPE_NV15); }
    static std::unique_ptr<HWComponent> create_nv20(const std::string &) { return std::make_unique<NvidiaNV>(NV_TYPE_NV20); }
    static std::unique_ptr<HWComponent> create_nv35(const std::string &) { return std::make_unique<NvidiaNV>(NV_TYPE_NV35); }
    static std::unique_ptr<HWComponent> create_nv40(const std::string &) { return std::make_unique<NvidiaNV>(NV_TYPE_NV40); }

    bool supports_io_space() override { return false; }
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);
    PostInitResultType device_postinit() override;

    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void     write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

    void vidc_draw_hw_cursor(uint8_t *dst_buf, int dst_pitch) override;
    void vidc_get_cursor_position(int &x, int &y) override;

    explicit NvidiaNV(NvCardType type);
    int32_t parse_child_unit_address_string(const std::string unit_address_string,
                                            HWComponent*& hwc) override;

protected:
    void notify_bar_change(int bar_num);
    void crtc_update();

private:
    void change_one_bar(uint32_t &aperture, uint32_t size, uint32_t new_base, int bar_num);

    uint32_t register_read32(uint32_t address);
    void     register_write32(uint32_t address, uint32_t value);
    uint8_t  register_read8(uint32_t address);
    void     register_write8(uint32_t address, uint8_t value);

    uint8_t  vram_read8(uint32_t a);
    uint16_t vram_read16(uint32_t a);
    uint32_t vram_read32(uint32_t a);
    uint64_t vram_read64(uint32_t a);
    void     vram_write8(uint32_t a, uint8_t v);
    void     vram_write16(uint32_t a, uint16_t v);
    void     vram_write32(uint32_t a, uint32_t v);
    void     vram_write64(uint32_t a, uint64_t v);

    uint8_t  ramin_read8(uint32_t a);
    uint16_t ramin_read16(uint32_t a);
    uint32_t ramin_read32(uint32_t a);
    void     ramin_write8(uint32_t a, uint8_t v);
    void     ramin_write32(uint32_t a, uint32_t v);

    uint8_t  physical_read8(uint32_t a);
    uint16_t physical_read16(uint32_t a);
    uint32_t physical_read32(uint32_t a);
    uint64_t physical_read64(uint32_t a);
    void     physical_write8(uint32_t a, uint8_t v);
    void     physical_write16(uint32_t a, uint16_t v);
    void     physical_write32(uint32_t a, uint32_t v);
    void     physical_write64(uint32_t a, uint64_t v);

    uint32_t dma_pt_lookup(uint32_t object, uint32_t address);
    uint32_t dma_lin_lookup(uint32_t object, uint32_t address);
    uint32_t dma_address(uint32_t object, uint32_t address);
    uint8_t  dma_read8(uint32_t object, uint32_t address);
    uint16_t dma_read16(uint32_t object, uint32_t address);
    uint32_t dma_read32(uint32_t object, uint32_t address);
    uint64_t dma_read64(uint32_t object, uint32_t address);
    void     dma_write8(uint32_t object, uint32_t address, uint8_t value);
    void     dma_write16(uint32_t object, uint32_t address, uint16_t value);
    void     dma_write32(uint32_t object, uint32_t address, uint32_t value);
    void     dma_write64(uint32_t object, uint32_t address, uint64_t value);
    void     dma_copy(uint32_t dst_obj, uint32_t dst_addr,
                      uint32_t src_obj, uint32_t src_addr, uint32_t byte_count);

    uint32_t ramfc_address(uint32_t chid, uint32_t offset);
    uint32_t ramfc_read32(uint32_t chid, uint32_t offset);
    void     ramfc_write32(uint32_t chid, uint32_t offset, uint32_t value);
    void     ramht_lookup(uint32_t handle, uint32_t chid, uint32_t *object, uint8_t *engine);

    uint64_t get_current_time();
    void     update_fifo_wait();
    void     fifo_process();
    void     fifo_process(uint32_t chid);
    int      execute_command(uint32_t chid, uint32_t subc, uint32_t method, uint32_t param);

    void     set_irq_level(bool level);
    uint32_t get_mc_intr();
    void     update_irq_level();

    void update_color_bytes_s2d(NvChannel *ch);
    void update_color_bytes_ifc(NvChannel *ch);
    void update_color_bytes_sifc(NvChannel *ch);
    void update_color_bytes_tfc(NvChannel *ch);
    void update_color_bytes_iifc(NvChannel *ch);
    void update_color_bytes(uint32_t s2d_fmt, uint32_t color_fmt, uint32_t *color_bytes);

    void execute_clip(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_m2mf(NvChannel *ch, uint32_t subc, uint32_t method, uint32_t param);
    void execute_rop(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_patt(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_gdi(NvChannel *ch, uint32_t cls, uint32_t method, uint32_t param);
    void execute_swzsurf(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_chroma(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_rect(NvChannel *ch, uint32_t method, uint32_t param);
    void do_rect(NvChannel *ch);
    void execute_imageblit(NvChannel *ch, uint32_t method, uint32_t param);
    void do_copyarea(NvChannel *ch);
    void execute_ifc(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_surf2d(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_iifc(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_sifc(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_beta(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_tfc(NvChannel *ch, uint32_t method, uint32_t param);
    void execute_sifm(NvChannel *ch, uint32_t cls, uint32_t method, uint32_t param);
    void execute_d3d(NvChannel *ch, uint32_t cls, uint32_t method, uint32_t param);

    uint32_t get_pixel(uint32_t obj, uint32_t ofs, uint32_t x, uint32_t cb);
    void     put_pixel(NvChannel *ch, uint32_t ofs, uint32_t x, uint32_t value);
    void     put_pixel_swzs(NvChannel *ch, uint32_t ofs, uint32_t value);
    void     pixel_operation(NvChannel *ch, uint32_t op,
                             uint32_t *dstcolor, const uint32_t *srccolor,
                             uint32_t cb, uint32_t px, uint32_t py);

    void gdi_fillrect(NvChannel *ch, bool clipped);
    void gdi_blit(NvChannel *ch, uint32_t type);
    void rect(NvChannel *ch);
    void ifc(NvChannel *ch, uint32_t word);
    void iifc(NvChannel *ch);
    void sifc(NvChannel *ch);
    void copyarea(NvChannel *ch);
    void tfc(NvChannel *ch);
    void m2mf(NvChannel *ch);
    void sifm(NvChannel *ch, bool swizzled);

    bool   d3d_scissor_clip(NvChannel *ch, uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);
    bool   d3d_viewport_clip(NvChannel *ch, uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);
    bool   d3d_window_clip(NvChannel *ch, uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h);
    void   d3d_clear_surface(NvChannel *ch);
    void   d3d_sample_texture(NvChannel *ch, NvTexture *tex, float coords_in[3], float color[4]);
    void   d3d_vertex_shader(NvChannel *ch, float in[16][4], float out[16][4]);
    void   d3d_register_combiners(NvChannel *ch, float regs[16][4], float out[4]);
    void   d3d_pixel_shader(NvChannel *ch, float in[16][4], float tmp16[64][4], float tmp32[64][4]);
    void   d3d_normal_to_view(NvChannel *ch, float n[3], float nt[3]);
    void   d3d_triangle(NvChannel *ch, uint32_t base);
    void   d3d_triangle_clipped(NvChannel *ch, float v0[16][4], float v1[16][4], float v2[16][4]);
    void   d3d_clip_to_screen(NvChannel *ch, float pos_clip[4], float pos_screen[4]);
    void   d3d_process_vertex(NvChannel *ch);
    void   d3d_load_vertex(NvChannel *ch, uint32_t index);
    uint32_t d3d_get_surface_pitch_z(NvChannel *ch);

    // ---- Hardware state ----

    NvCardType  card_type;
    uint32_t    memsize;
    uint32_t    memsize_mask;
    uint32_t    bar2_size;
    uint32_t    ramin_flip;
    uint32_t    class_mask;

    std::unique_ptr<uint8_t[]> vram_ptr;

    // BAR layout — three entries, bar2_size==0 means BAR2 absent
    // aperture_flag bit 3 = prefetchable (set on VRAM BAR1)
    static constexpr int NV_BAR_COUNT = 3;
    uint32_t aperture_size[NV_BAR_COUNT] = {};
    uint32_t aperture_flag[NV_BAR_COUNT] = {};
    uint32_t aperture_base[NV_BAR_COUNT] = {};

    bool     mc_soft_intr = false;
    bool     big_endian_mode = false;  // NV_PMC_BOOT_1 bit 0
    bool     saw_user_dma_put = false;
    uint32_t mc_intr_en = 0, mc_enable = 0;
    uint32_t bus_intr = 0, bus_intr_en = 0;

    bool     fifo_wait = false, fifo_wait_soft = false;
    bool     fifo_wait_notify = false, fifo_wait_flip = false, fifo_wait_acquire = false;
    uint32_t fifo_intr = 0, fifo_intr_en = 0;
    uint32_t fifo_ramht = 0, fifo_ramfc = 0, fifo_ramro = 0, fifo_mode = 0;
    uint32_t fifo_cache1_push0 = 0, fifo_cache1_push1 = 0, fifo_cache1_put = 0;
    uint32_t fifo_cache1_dma_push = 0, fifo_cache1_dma_instance = 0;
    uint32_t fifo_cache1_dma_put = 0, fifo_cache1_dma_get = 0;
    uint32_t fifo_cache1_ref_cnt = 0, fifo_cache1_pull0 = 0;
    uint32_t fifo_cache1_semaphore = 0, fifo_cache1_get = 0;
    uint32_t fifo_grctx_instance = 0;
    uint32_t fifo_cache1_method[NV_CACHE1_SIZE] = {};
    uint32_t fifo_cache1_data[NV_CACHE1_SIZE]   = {};

    uint32_t rma_addr = 0;
    uint32_t timer_intr = 0, timer_intr_en = 0;
    uint32_t timer_num = 0, timer_den = 0, timer_alarm = 0;
    uint64_t timer_inittime1 = 0, timer_inittime2 = 0;

    uint32_t straps0_primary = 0, straps0_primary_original = 0;

    uint32_t graph_intr = 0, graph_nsource = 0, graph_intr_en = 0;
    uint32_t graph_ctx_switch1 = 0, graph_ctx_switch2 = 0, graph_ctx_switch4 = 0;
    uint32_t graph_ctxctl_cur = 0, graph_status = 0;
    uint32_t graph_trapped_addr = 0, graph_trapped_data = 0;
    uint32_t graph_flip_read = 0, graph_flip_write = 0, graph_flip_modulo = 0;
    uint32_t graph_notify = 0, graph_fifo = 0, graph_bpixel = 0;
    uint32_t graph_channel_ctx_table = 0, graph_offset0 = 0, graph_pitch0 = 0;

    uint32_t crtc_intr = 0, crtc_intr_en = 0, crtc_start = 0;
    std::chrono::steady_clock::time_point vbl_last_ack = std::chrono::steady_clock::now();
    bool vbl_in_blank = false;
    bool vbl_pending = false;  // true from VBlank rising edge until falling edge
    uint32_t crtc_config = 0, crtc_raster_pos = 0;
    uint32_t crtc_cursor_offset = 0, crtc_cursor_config = 0, crtc_gpio_ext = 0;

    uint32_t ramdac_cu_start_pos = 0, ramdac_nvpll = 0, ramdac_mpll = 0;
    uint32_t ramdac_vpll = 0, ramdac_vpll_b = 0;
    uint32_t ramdac_pll_select = 0, ramdac_general_control = 0;

    struct { uint8_t index; uint8_t reg[NV_CRTC_EXT_MAX + 1]; } crtc_ext = {};

    // VGA register mirrors (PRMVIO / PRMCIO)
    uint8_t vga_misc_output = 0;
    uint8_t vga_enable = 0;
    uint8_t vga_seq_index = 0, vga_seq_data[8] = {};
    uint8_t vga_attr_index = 0, vga_attr_data[0x20] = {};
    bool    vga_attr_flip = false;
    uint8_t vga_gfx_index = 0, vga_gfx_data[16] = {};

    // VGA DAC palette
    uint8_t dac_wr_index = 0, dac_rd_index = 0, dac_state = 0;
    uint8_t dac_comp = 0;  // 0=R, 1=G, 2=B
    uint8_t dac_mask = 0xFF;
    uint8_t dac_rgb[256][3] = {};

    NvChannel chs[NV_CHANNEL_COUNT];
    uint32_t  unk_regs[4 * 1024 * 1024];   // catch-all; shrink as coverage grows

    unsigned disp_xres = 640, disp_yres = 480, disp_bpp = 8, disp_pitch = 640;
    uint8_t *disp_ptr = nullptr;
    uint32_t disp_offset = 0;
    bool     mode_needs_update = false, double_width = false;

    struct {
        bool vram = false; uint32_t offset = 0;
        int16_t x = 0, y = 0; uint8_t size = 32;
        bool bpp32 = false, enabled = false;
    } hw_cursor;

    DisplayID* disp_id = nullptr;

    // Custom I2C DDC state machine
    enum DdcState { DDC_IDLE, DDC_START, DDC_ADDR, DDC_ACK_ADDR, DDC_REG, DDC_ACK_REG, DDC_DATA, DDC_ACK_DATA, DDC_NACK };
    struct {
        DdcState state = DDC_IDLE;
        uint8_t byte = 0, bit_count = 0;
        uint8_t dev_addr = 0, reg_addr = 0;
        uint16_t data_pos = 0;
        bool last_scl = true, last_sda = true;
        bool master_sda = true;  // raw master SDA level
        bool device_sda = true;
        bool stop_suppress = false;
    } ddc_i2c;
    void     ddc_i2c_write(bool scl, bool sda);
    uint8_t  ddc_i2c_read();
};

#endif // NVIDIA_NV_H



// ---- Added NV2x scanout simulation ----
#include <thread>
#include <atomic>

extern std::atomic<int> nv_scanline;
extern std::atomic<bool> nv_vblank;

void start_nv_scanout();
void stop_nv_scanout();
// --------------------------------------
