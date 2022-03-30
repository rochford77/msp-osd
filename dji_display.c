#include <stdlib.h>
#include "dji_display.h"

static duss_result_t pop_func(duss_disp_instance_handle_t *disp_handle,duss_disp_plane_id_t plane_id, duss_frame_buffer_t *frame_buffer,void *user_ctx) {
    return 0;
}

dji_display_state_t *dji_display_state_alloc() {
    dji_display_state_t *display_state = calloc(1, sizeof(dji_display_state_t));
    display_state->disp_instance_handle = (duss_disp_instance_handle_t *)calloc(1, sizeof(duss_disp_instance_handle_t));
    display_state->fb_0 = (duss_frame_buffer_t *)calloc(1,sizeof(duss_frame_buffer_t));
    display_state->fb_1 = (duss_frame_buffer_t *)calloc(1,sizeof(duss_frame_buffer_t));
    display_state->pb_0 = (duss_disp_plane_blending_t *)calloc(1, sizeof(duss_disp_plane_blending_t));
    return display_state;
}

void dji_display_state_free(dji_display_state_t *display_state) {
    free(display_state->disp_instance_handle);
    free(display_state->fb_0);
    free(display_state->fb_1);
    free(display_state->pb_0);
    free(display_state);
}

void dji_display_close_framebuffer(dji_display_state_t *display_state) {
    duss_hal_mem_free(display_state->ion_buf_0);
    duss_hal_mem_free(display_state->ion_buf_1);
    duss_hal_display_release_plane(display_state->disp_instance_handle, display_state->plane_id);
    duss_hal_device_close(display_state->disp_handle);
    duss_hal_device_stop(display_state->ion_handle);
    duss_hal_device_close(display_state->ion_handle);
    duss_hal_deinitialize();
}

void dji_display_open_framebuffer(dji_display_state_t *display_state, duss_disp_plane_id_t plane_id) {
    uint32_t hal_device_open_unk = 0;
    duss_result_t res = 0;

    display_state->plane_id = plane_id;

    // PLANE BLENDING

    display_state->pb_0->voffset = 575; // this is either 215 or 575 depending on hwid?
    display_state->pb_0->hoffset = 0;
    display_state->pb_0->order = 2;
    display_state->pb_0->is_enable = 1;
    display_state->pb_0->glb_alpha_en = 0;
    display_state->pb_0->glb_alpha_val = 0;
    display_state->pb_0->blending_alg = 1;
    
    duss_hal_device_desc_t device_descs[3] = {
        {"/dev/dji_display", &duss_hal_attach_disp, &duss_hal_detach_disp, 0x0},
        {"/dev/ion", &duss_hal_attach_ion_mem, &duss_hal_detach_ion_mem, 0x0},
        {0,0,0,0}
    };

    duss_hal_initialize(device_descs);
    
    res = duss_hal_device_open("/dev/dji_display",&hal_device_open_unk,&display_state->disp_handle);
    if (res != 0) {
        printf("failed to open dji_display device");
        exit(0);
    }
    res = duss_hal_display_open(display_state->disp_handle, &display_state->disp_instance_handle, 0);
    if (res != 0) {
        printf("failed to open display hal");
        exit(0);
    }
    res = duss_hal_display_reset(display_state->disp_instance_handle);
    if (res != 0) {
        printf("failed to reset display");
        exit(0);
    }
    res = duss_hal_display_aquire_plane(display_state->disp_instance_handle,0,&plane_id);
    if (res != 0) {
        printf("failed to acquire plane");
        exit(0);
    }
    res = duss_hal_display_register_frame_cycle_callback(display_state->disp_instance_handle, plane_id, &pop_func, 0);
    if (res != 0) {
        printf("failed to register callback");
        exit(0);
    }
    res = duss_hal_display_port_enable(display_state->disp_instance_handle, 3, 1);
    if (res != 0) {
        printf("failed to enable display port");
        exit(0);
    }

    res = duss_hal_display_plane_blending_set(display_state->disp_instance_handle, plane_id, display_state->pb_0);
    
    if (res != 0) {
        printf("failed to set blending");
        exit(0);
    }
    res = duss_hal_device_open("/dev/ion", &hal_device_open_unk, &display_state->ion_handle);
    if (res != 0) {
        printf("failed to open shared VRAM");
        exit(0);
    }
    res = duss_hal_device_start(display_state->ion_handle,0);
    if (res != 0) {
        printf("failed to start VRAM device");
        exit(0);
    }

    res = duss_hal_mem_alloc(display_state->ion_handle,&display_state->ion_buf_0,0x473100,0x400,0,0x17);
    if (res != 0) {
        printf("failed to allocate VRAM");
        exit(0);
    }
    res = duss_hal_mem_map(display_state->ion_buf_0, &display_state->fb0_virtual_addr);
    if (res != 0) {
        printf("failed to map VRAM");
        exit(0);
    }
    res = duss_hal_mem_get_phys_addr(display_state->ion_buf_0, &display_state->fb0_physical_addr);
    if (res != 0) {
        printf("failed ot get phys addr");
        exit(0);
    }
    printf("first buffer VRAM mapped virtual memory is at %p : %p\n", display_state->fb0_virtual_addr, display_state->fb0_physical_addr);

    res = duss_hal_mem_alloc(display_state->ion_handle,&display_state->ion_buf_1,0x473100,0x400,0,0x17);
    if (res != 0) {
        printf("failed to allocate VRAM");
        exit(0);
    }
    res = duss_hal_mem_map(display_state->ion_buf_1,&display_state->fb1_virtual_addr);
    if (res != 0) {
        printf("failed to map VRAM");
        exit(0);
    }
    res = duss_hal_mem_get_phys_addr(display_state->ion_buf_1, &display_state->fb1_physical_addr);
    if (res != 0) {
        printf("failed ot get phys addr");
        exit(0);
    }
    printf("second buffer VRAM mapped virtual memory is at %p : %p\n", display_state->fb1_virtual_addr, display_state->fb1_physical_addr);
    
    for(int i = 0; i < 2; i++) {
        duss_frame_buffer_t *fb = i ? display_state->fb_1 : display_state->fb_0;
        fb->buffer = i ? display_state->ion_buf_1 : display_state->ion_buf_0;
        fb->pixel_format = DUSS_PIXFMT_RGBA8888;
        fb->frame_id = i;
        fb->planes[0].bytes_per_line = 0x1680;
        fb->planes[0].offset = 0;
        fb->planes[0].plane_height = 810;
        fb->planes[0].bytes_written = 0x473100;
        fb->width = 1440;
        fb->height = 810;
        fb->plane_count = 1;
    }
}

void dji_display_push_frame(dji_display_state_t *display_state, duss_frame_buffer_t *fb) {
        duss_result_t res = 0;
        duss_hal_mem_sync(fb->buffer, 1);
        res = duss_hal_display_push_frame(display_state->disp_instance_handle, display_state->plane_id, fb);
}