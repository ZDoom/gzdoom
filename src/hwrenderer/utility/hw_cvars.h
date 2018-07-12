#pragma once


#include "r_defs.h"
#include "c_cvars.h"

EXTERN_CVAR(Bool,gl_enhanced_nightvision)
EXTERN_CVAR(Int, screenblocks);
EXTERN_CVAR(Bool, gl_texture)
EXTERN_CVAR(Int, gl_texture_filter)
EXTERN_CVAR(Float, gl_texture_filter_anisotropic)
EXTERN_CVAR(Int, gl_texture_format)
EXTERN_CVAR(Bool, gl_texture_usehires)
EXTERN_CVAR(Bool, gl_usefb)

EXTERN_CVAR(Int, gl_weaponlight)

EXTERN_CVAR (Bool, gl_lights);
EXTERN_CVAR (Bool, gl_attachedlights);

EXTERN_CVAR (Bool, gl_light_sprites);
EXTERN_CVAR (Bool, gl_light_particles);
EXTERN_CVAR (Bool, gl_light_shadowmap);
EXTERN_CVAR (Int, gl_shadowmap_quality);

EXTERN_CVAR(Int, gl_fogmode)
EXTERN_CVAR(Int, gl_lightmode)
EXTERN_CVAR(Bool,gl_mirror_envmap)

EXTERN_CVAR(Bool,gl_mirrors)
EXTERN_CVAR(Bool,gl_mirror_envmap)
EXTERN_CVAR(Bool, gl_seamless)

EXTERN_CVAR(Float, gl_mask_threshold)
EXTERN_CVAR(Float, gl_mask_sprite_threshold)

EXTERN_CVAR(Bool, gl_renderbuffers)
EXTERN_CVAR(Int, gl_multisample)

EXTERN_CVAR(Bool, gl_bloom)
EXTERN_CVAR(Float, gl_bloom_amount)
EXTERN_CVAR(Int, gl_bloom_kernel_size)
EXTERN_CVAR(Int, gl_tonemap)
EXTERN_CVAR(Float, gl_exposure)
EXTERN_CVAR(Bool, gl_lens)
EXTERN_CVAR(Float, gl_lens_k)
EXTERN_CVAR(Float, gl_lens_kcube)
EXTERN_CVAR(Float, gl_lens_chromatic)
EXTERN_CVAR(Int, gl_ssao)
EXTERN_CVAR(Int, gl_ssao_portals)
EXTERN_CVAR(Float, gl_ssao_strength)
EXTERN_CVAR(Int, gl_ssao_debug)
EXTERN_CVAR(Float, gl_ssao_bias)
EXTERN_CVAR(Float, gl_ssao_radius)
EXTERN_CVAR(Float, gl_ssao_blur_amount)

EXTERN_CVAR(Int, gl_debug_level)
EXTERN_CVAR(Bool, gl_debug_breakpoint)


EXTERN_CVAR(Bool, gl_usecolorblending)
EXTERN_CVAR(Bool, gl_spritebrightfog)
EXTERN_CVAR(Bool, gl_sprite_blend)
EXTERN_CVAR(Int, gl_spriteclip)
EXTERN_CVAR(Float, gl_sclipthreshold)
EXTERN_CVAR(Float, gl_sclipfactor)
EXTERN_CVAR(Int, gl_particles_style)
EXTERN_CVAR(Int, gl_billboard_mode)
EXTERN_CVAR(Bool, gl_billboard_faces_camera)
EXTERN_CVAR(Bool, gl_billboard_particles)
EXTERN_CVAR(Int, gl_enhanced_nv_stealth)
EXTERN_CVAR(Int, gl_fuzztype)
