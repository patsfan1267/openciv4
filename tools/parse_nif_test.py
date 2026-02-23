#!/usr/bin/env python3
"""
NIF Binary Format Parser for Civilization IV: Beyond the Sword
==============================================================
Version: 20.0.0.4 (0x14000004), Gamebryo, user_version=0

Parses NIF files to document the exact binary layout for a C++ parser.
All format details derived from niftools/nif.xml and verified against real files.

KEY FORMAT FACTS FOR VERSION 20.0.0.4 (user_version=0):
  - NO user_version_2 field (only if user_version >= 10)
  - NO export info strings (only if user_version >= 10)
  - NO block size array (only version >= 20.2.0.7)
  - NO string table (only version >= 20.1.0.3)
  - Strings are inline: uint32 length + chars
  - Bools are 1 byte (uint8) since version >= 4.1.0.1
  - Flags are uint16 (2 bytes)
  - Refs are int32 (4 bytes), -1 = none
  - Unknown Int 2 (uint32) appears AFTER block type indices
  - Must parse blocks sequentially (no block sizes in header)
"""

import struct
import sys
import os

# ============================================================================
# BINARY READER
# ============================================================================

class BinaryReader:
    def __init__(self, f):
        self.f = f

    def tell(self):
        return self.f.tell()

    def seek(self, pos):
        self.f.seek(pos)

    def read_bytes(self, n):
        data = self.f.read(n)
        if len(data) < n:
            raise EOFError(f"Wanted {n} bytes at offset {self.tell()-len(data)}, got {len(data)}")
        return data

    def read_uint8(self):
        return struct.unpack('<B', self.read_bytes(1))[0]

    def read_int8(self):
        return struct.unpack('<b', self.read_bytes(1))[0]

    def read_uint16(self):
        return struct.unpack('<H', self.read_bytes(2))[0]

    def read_int16(self):
        return struct.unpack('<h', self.read_bytes(2))[0]

    def read_uint32(self):
        return struct.unpack('<I', self.read_bytes(4))[0]

    def read_int32(self):
        return struct.unpack('<i', self.read_bytes(4))[0]

    def read_float(self):
        return struct.unpack('<f', self.read_bytes(4))[0]

    def read_bool(self):
        """NIF bool: 1 byte for version >= 4.1.0.1"""
        return self.read_uint8() != 0

    def read_string(self):
        """SizedString: uint32 length + chars (no null terminator)"""
        length = self.read_uint32()
        if length == 0:
            return ""
        if length > 100000:
            raise ValueError(f"String too long: {length} at offset {self.tell()-4}")
        return self.read_bytes(length).decode('ascii', errors='replace')

    def read_ref(self):
        """Block reference: int32, -1 = none"""
        return self.read_int32()

    def read_vec3(self):
        return (self.read_float(), self.read_float(), self.read_float())

    def read_color3(self):
        return self.read_vec3()

    def read_color4(self):
        return (self.read_float(), self.read_float(), self.read_float(), self.read_float())

    def read_matrix33(self):
        """3x3 rotation matrix, row-major (9 floats)"""
        return [self.read_float() for _ in range(9)]

    def read_flags(self):
        """Flags: uint16"""
        return self.read_uint16()


# ============================================================================
# HEADER
# ============================================================================

class NifHeader:
    pass


def parse_header(br):
    hdr = NifHeader()

    # Header string ending with 0x0A
    header_bytes = b""
    while True:
        b = br.read_bytes(1)
        if b == b'\n':
            break
        header_bytes += b
    hdr.header_string = header_bytes.decode('ascii', errors='replace')

    hdr.version = br.read_uint32()
    hdr.endian = br.read_uint8()
    hdr.user_version = br.read_uint32()
    hdr.num_blocks = br.read_uint32()

    # For version 20.0.0.4, user_version=0:
    # NO user_version_2, NO export info

    hdr.num_block_types = br.read_uint16()
    hdr.block_type_names = [br.read_string() for _ in range(hdr.num_block_types)]
    hdr.block_type_indices = [br.read_uint16() for _ in range(hdr.num_blocks)]

    # Unknown Int 2 (present for version >= 10.0.1.0)
    hdr.unknown_int_2 = br.read_uint32()

    return hdr


def print_header(hdr):
    va = (hdr.version >> 24) & 0xFF
    vb = (hdr.version >> 16) & 0xFF
    vc = (hdr.version >> 8) & 0xFF
    vd = hdr.version & 0xFF

    print("=" * 80)
    print("NIF HEADER")
    print("=" * 80)
    print(f"  Header string:    {hdr.header_string}")
    print(f"  Version:          {va}.{vb}.{vc}.{vd} (0x{hdr.version:08X})")
    print(f"  Endian:           {'Little' if hdr.endian == 1 else 'Big'}")
    print(f"  User version:     {hdr.user_version}")
    print(f"  Num blocks:       {hdr.num_blocks}")
    print(f"  Unknown Int 2:    {hdr.unknown_int_2}")
    print(f"  Num block types:  {hdr.num_block_types}")
    print()

    print("  Block Type Names:")
    for i, name in enumerate(hdr.block_type_names):
        print(f"    [{i:3d}] {name}")
    print()

    # Block summary
    type_counts = {}
    for i in range(hdr.num_blocks):
        idx = hdr.block_type_indices[i]
        name = hdr.block_type_names[idx]
        type_counts[name] = type_counts.get(name, 0) + 1

    print("  Block Layout (first 50):")
    for i in range(min(hdr.num_blocks, 50)):
        idx = hdr.block_type_indices[i]
        name = hdr.block_type_names[idx]
        print(f"    [{i:4d}] {name}")
    if hdr.num_blocks > 50:
        print(f"    ... ({hdr.num_blocks - 50} more)")
    print()

    print("  Type Counts:")
    for name, count in sorted(type_counts.items(), key=lambda x: -x[1]):
        print(f"    {name:40s} x{count}")
    print()


# ============================================================================
# COMMON FIELDS
# ============================================================================

def parse_ni_object_net(br):
    """NiObjectNET: name, extra data list, controller ref"""
    info = {}
    info['name'] = br.read_string()
    num_extra = br.read_uint32()
    info['num_extra_data'] = num_extra
    info['extra_data_refs'] = [br.read_ref() for _ in range(num_extra)]
    info['controller_ref'] = br.read_ref()
    return info


def parse_ni_av_object(br):
    """NiAVObject: NiObjectNET + flags, transform, properties, collision"""
    info = parse_ni_object_net(br)
    info['flags'] = br.read_flags()
    info['translation'] = br.read_vec3()
    info['rotation'] = br.read_matrix33()
    info['scale'] = br.read_float()
    num_props = br.read_uint32()
    info['num_properties'] = num_props
    info['property_refs'] = [br.read_ref() for _ in range(num_props)]
    info['collision_ref'] = br.read_ref()
    return info


# ============================================================================
# BLOCK PARSERS
# ============================================================================

def parse_NiNode(br, idx):
    """NiNode: NiAVObject + children + effects"""
    start = br.tell()
    info = parse_ni_av_object(br)

    num_ch = br.read_uint32()
    info['num_children'] = num_ch
    info['children_refs'] = [br.read_ref() for _ in range(num_ch)]

    num_eff = br.read_uint32()
    info['num_effects'] = num_eff
    info['effects_refs'] = [br.read_ref() for _ in range(num_eff)]

    size = br.tell() - start
    print(f"\n  --- NiNode [block {idx}] ({size} bytes) ---")
    print(f"  name='{info['name']}', flags=0x{info['flags']:04X}")
    t = info['translation']
    print(f"  translation=({t[0]:.4f}, {t[1]:.4f}, {t[2]:.4f}), scale={info['scale']:.4f}")
    print(f"  properties={info['property_refs']}, collision={info['collision_ref']}")
    print(f"  children={info['children_refs']}")
    print(f"  effects={info['effects_refs']}")
    print(f"  extra_data={info['extra_data_refs']}, controller={info['controller_ref']}")
    return info, size


def parse_NiTriShape(br, idx):
    """NiTriShape: NiAVObject + data ref + skin ref + has_shader + shader_name + unknown_int"""
    start = br.tell()
    info = parse_ni_av_object(br)

    info['data_ref'] = br.read_ref()
    info['skin_instance_ref'] = br.read_ref()

    # NiGeometry fields for version 10.0.1.0 to 20.1.0.3:
    info['has_shader'] = br.read_bool()
    if info['has_shader']:
        info['shader_name'] = br.read_string()
        info['shader_unknown_int'] = br.read_int32()
    else:
        info['shader_name'] = ""
        info['shader_unknown_int'] = 0

    size = br.tell() - start
    print(f"\n  --- NiTriShape [block {idx}] ({size} bytes) ---")
    print(f"  name='{info['name']}', flags=0x{info['flags']:04X}")
    t = info['translation']
    print(f"  translation=({t[0]:.4f}, {t[1]:.4f}, {t[2]:.4f}), scale={info['scale']:.4f}")
    print(f"  properties={info['property_refs']}")
    print(f"  data_ref={info['data_ref']}, skin_instance={info['skin_instance_ref']}")
    print(f"  has_shader={info['has_shader']}, shader='{info['shader_name']}'")
    return info, size


def parse_NiTriStrips(br, idx):
    """NiTriStrips: same as NiTriShape (same inheritance chain)"""
    start = br.tell()
    info = parse_ni_av_object(br)

    info['data_ref'] = br.read_ref()
    info['skin_instance_ref'] = br.read_ref()

    info['has_shader'] = br.read_bool()
    if info['has_shader']:
        info['shader_name'] = br.read_string()
        info['shader_unknown_int'] = br.read_int32()

    size = br.tell() - start
    print(f"\n  --- NiTriStrips [block {idx}] ({size} bytes) ---")
    print(f"  name='{info['name']}', flags=0x{info['flags']:04X}")
    t = info['translation']
    print(f"  translation=({t[0]:.4f}, {t[1]:.4f}, {t[2]:.4f}), scale={info['scale']:.4f}")
    print(f"  properties={info['property_refs']}")
    print(f"  data_ref={info['data_ref']}, skin_instance={info['skin_instance_ref']}")
    print(f"  has_shader={info['has_shader']}")
    return info, size


def parse_NiGeometryData_common(br):
    """Parse NiGeometryData fields (base of NiTriShapeData/NiTriStripsData)"""
    info = {}

    # Unknown Int (present for version >= 10.2.0.0)
    info['group_id'] = br.read_int32()
    info['num_vertices'] = br.read_uint16()
    nv = info['num_vertices']

    # Keep/Compress flags (version >= 10.1.0.0)
    info['keep_flags'] = br.read_uint8()
    info['compress_flags'] = br.read_uint8()

    info['has_vertices'] = br.read_bool()
    info['vertices'] = []
    if info['has_vertices']:
        for _ in range(nv):
            info['vertices'].append(br.read_vec3())

    # Num UV Sets (byte for version >= 10.0.1.0) and Extra Vectors Flags (byte)
    info['num_uv_sets_byte'] = br.read_uint8()   # byte, not uint16!
    info['extra_vectors_flags'] = br.read_uint8()  # bit 4 = has tangents/bitangents

    info['has_normals'] = br.read_bool()
    info['normals'] = []
    if info['has_normals']:
        for _ in range(nv):
            info['normals'].append(br.read_vec3())

    # Tangents and Bitangents if has_normals AND bit 4 of extra_vectors_flags
    info['tangents'] = []
    info['bitangents'] = []
    if info['has_normals'] and (info['extra_vectors_flags'] & 0x10):
        for _ in range(nv):
            info['tangents'].append(br.read_vec3())
        for _ in range(nv):
            info['bitangents'].append(br.read_vec3())

    # Bounding sphere
    info['center'] = br.read_vec3()
    info['radius'] = br.read_float()

    # Vertex colors
    info['has_vertex_colors'] = br.read_bool()
    info['vertex_colors'] = []
    if info['has_vertex_colors']:
        for _ in range(nv):
            info['vertex_colors'].append(br.read_color4())

    # UV sets: num_uv_sets_byte tells how many, data follows immediately
    actual_uv_count = info['num_uv_sets_byte'] & 0x3F
    info['uv_sets'] = []
    for uv_set in range(actual_uv_count):
        uvs = []
        for _ in range(nv):
            u = br.read_float()
            v = br.read_float()
            uvs.append((u, v))
        info['uv_sets'].append(uvs)

    # Consistency flags (version >= 10.0.1.0)
    info['consistency_flags'] = br.read_uint16()

    # Additional data ref (version >= 20.0.0.4)
    info['additional_data_ref'] = br.read_ref()

    return info


def parse_NiTriShapeData(br, idx):
    """NiTriShapeData: NiGeometryData + triangles"""
    start = br.tell()
    info = parse_NiGeometryData_common(br)
    nv = info['num_vertices']

    # NiTriBasedGeomData: num_triangles (uint16)
    info['num_triangles'] = br.read_uint16()

    # NiTriShapeData-specific:
    info['num_triangle_points'] = br.read_uint32()
    info['has_triangles'] = br.read_bool()
    info['triangles'] = []
    if info['has_triangles']:
        for _ in range(info['num_triangles']):
            v0 = br.read_uint16()
            v1 = br.read_uint16()
            v2 = br.read_uint16()
            info['triangles'].append((v0, v1, v2))

    info['num_match_groups'] = br.read_uint16()
    for _ in range(info['num_match_groups']):
        mg_count = br.read_uint16()
        for _ in range(mg_count):
            br.read_uint16()

    size = br.tell() - start
    actual_uv = info['num_uv_sets_byte'] & 0x3F

    print(f"\n  --- NiTriShapeData [block {idx}] ({size} bytes) ---")
    print(f"  num_vertices={nv}, has_vertices={info['has_vertices']}")
    if info['vertices'] and nv > 0:
        for vi in range(min(3, nv)):
            v = info['vertices'][vi]
            print(f"    vert[{vi}] ({v[0]:.4f}, {v[1]:.4f}, {v[2]:.4f})")
    print(f"  has_normals={info['has_normals']}")
    if info['normals'] and nv > 0:
        for vi in range(min(2, nv)):
            n = info['normals'][vi]
            print(f"    norm[{vi}] ({n[0]:.4f}, {n[1]:.4f}, {n[2]:.4f})")
    print(f"  tangents={'yes' if info['tangents'] else 'no'} (extra_vectors_flags=0x{info['extra_vectors_flags']:02X})")
    c = info['center']
    print(f"  bounding: center=({c[0]:.4f}, {c[1]:.4f}, {c[2]:.4f}), radius={info['radius']:.4f}")
    print(f"  has_vertex_colors={info['has_vertex_colors']}")
    print(f"  num_uv_sets={actual_uv}")
    for uv_idx, uvs in enumerate(info['uv_sets']):
        if uvs:
            for vi in range(min(2, len(uvs))):
                print(f"    uv[{uv_idx}][{vi}] ({uvs[vi][0]:.4f}, {uvs[vi][1]:.4f})")
    print(f"  num_triangles={info['num_triangles']}, num_tri_points={info['num_triangle_points']}")
    if info['triangles']:
        for ti in range(min(3, len(info['triangles']))):
            t = info['triangles'][ti]
            print(f"    tri[{ti}] ({t[0]}, {t[1]}, {t[2]})")
    print(f"  num_match_groups={info['num_match_groups']}")
    return info, size


def parse_NiTriStripsData(br, idx):
    """NiTriStripsData: NiGeometryData + strips"""
    start = br.tell()
    info = parse_NiGeometryData_common(br)
    nv = info['num_vertices']

    # NiTriBasedGeomData: num_triangles (uint16)
    info['num_triangles'] = br.read_uint16()

    # NiTriStripsData-specific:
    info['num_strips'] = br.read_uint16()
    info['strip_lengths'] = [br.read_uint16() for _ in range(info['num_strips'])]

    # Has Points (bool) for version >= 10.0.1.3
    info['has_points'] = br.read_bool()
    info['strips'] = []
    if info['has_points']:
        for si in range(info['num_strips']):
            strip = [br.read_uint16() for _ in range(info['strip_lengths'][si])]
            info['strips'].append(strip)

    size = br.tell() - start
    actual_uv = info['num_uv_sets_byte'] & 0x3F

    print(f"\n  --- NiTriStripsData [block {idx}] ({size} bytes) ---")
    print(f"  num_vertices={nv}, has_vertices={info['has_vertices']}")
    if info['vertices'] and nv > 0:
        for vi in range(min(3, nv)):
            v = info['vertices'][vi]
            print(f"    vert[{vi}] ({v[0]:.4f}, {v[1]:.4f}, {v[2]:.4f})")
    print(f"  has_normals={info['has_normals']}, tangents={'yes' if info['tangents'] else 'no'}")
    c = info['center']
    print(f"  bounding: center=({c[0]:.4f}, {c[1]:.4f}, {c[2]:.4f}), radius={info['radius']:.4f}")
    print(f"  has_vertex_colors={info['has_vertex_colors']}, num_uv_sets={actual_uv}")
    print(f"  num_triangles={info['num_triangles']}, num_strips={info['num_strips']}")
    print(f"  strip_lengths={info['strip_lengths'][:10]}{'...' if len(info['strip_lengths'])>10 else ''}")
    if info['strips']:
        for si in range(min(2, len(info['strips']))):
            s = info['strips'][si]
            print(f"    strip[{si}] ({len(s)} pts): {s[:8]}{'...' if len(s)>8 else ''}")
    return info, size


def parse_NiTexturingProperty(br, idx):
    """NiTexturingProperty"""
    start = br.tell()
    info = parse_ni_object_net(br)

    # For version <= 20.0.0.5: Apply Mode (uint32)
    info['apply_mode'] = br.read_uint32()

    info['texture_count'] = br.read_uint32()

    slot_names = ['Base(Diffuse)', 'Dark', 'Detail', 'Gloss', 'Glow(Emissive)',
                  'BumpMap', 'Decal0']

    print(f"\n  --- NiTexturingProperty [block {idx}] ---")
    print(f"  name='{info['name']}', apply_mode={info['apply_mode']}")
    print(f"  texture_count={info['texture_count']}")

    def read_tex_desc():
        """Read a TexDesc compound"""
        td = {}
        td['source_ref'] = br.read_ref()
        td['clamp_mode'] = br.read_uint32()  # TexClampMode storage=uint (4 bytes)
        td['filter_mode'] = br.read_uint32()  # TexFilterMode storage=uint (4 bytes)
        td['uv_set'] = br.read_uint32()      # ver2 = 20.0.0.5
        td['has_tex_transform'] = br.read_bool()
        if td['has_tex_transform']:
            td['translation'] = (br.read_float(), br.read_float())
            td['tiling'] = (br.read_float(), br.read_float())
            td['w_rotation'] = br.read_float()
            td['transform_type'] = br.read_uint32()
            td['center_offset'] = (br.read_float(), br.read_float())
        return td

    # Standard texture slots
    # The order is always: Base, Dark, Detail, Gloss, Glow, BumpMap, [Decal0]
    # Each has: bool has_tex, then TexDesc if true
    # BumpMap has extra fields after TexDesc

    info['slots'] = {}

    # Slots 0-4: Base, Dark, Detail, Gloss, Glow
    for slot_idx in range(min(info['texture_count'], 5)):
        slot_name = slot_names[slot_idx] if slot_idx < len(slot_names) else f'Slot{slot_idx}'
        has_tex = br.read_bool()
        if has_tex:
            td = read_tex_desc()
            info['slots'][slot_name] = td
            clamp_str = {0:'CLAMP_S_CLAMP_T', 1:'CLAMP_S_WRAP_T', 2:'WRAP_S_CLAMP_T', 3:'WRAP_S_WRAP_T'}.get(td['clamp_mode'], str(td['clamp_mode']))
            filter_str = {0:'NEAREST', 1:'BILERP', 2:'TRILERP', 3:'NEAR_MIP_NEAR', 4:'NEAR_MIP_LIN', 5:'LIN_MIP_NEAR'}.get(td['filter_mode'], str(td['filter_mode']))
            print(f"  [{slot_idx}] {slot_name}: source={td['source_ref']}, clamp={clamp_str}, filter={filter_str}, uv_set={td['uv_set']}, transform={td['has_tex_transform']}")
        else:
            print(f"  [{slot_idx}] {slot_name}: (none)")

    # Slot 5: BumpMap (has extra float/float/matrix22 fields)
    if info['texture_count'] > 5:
        has_bump = br.read_bool()
        if has_bump:
            td = read_tex_desc()
            info['bump_luma_scale'] = br.read_float()
            info['bump_luma_offset'] = br.read_float()
            # Matrix22: 4 floats
            info['bump_matrix'] = [br.read_float() for _ in range(4)]
            info['slots']['BumpMap'] = td
            print(f"  [5] BumpMap: source={td['source_ref']}, luma_scale={info['bump_luma_scale']:.2f}, luma_offset={info['bump_luma_offset']:.2f}")
        else:
            print(f"  [5] BumpMap: (none)")

    # Slot 6+: Decals
    # For version <= 20.1.0.3: Decal0 if texture_count >= 7
    if info['texture_count'] >= 7:
        has_decal0 = br.read_bool()
        if has_decal0:
            td = read_tex_desc()
            info['slots']['Decal0'] = td
            print(f"  [6] Decal0: source={td['source_ref']}")
        else:
            print(f"  [6] Decal0: (none)")

    # Decal1 if texture_count >= 8 (and version <= 20.1.0.3)
    if info['texture_count'] >= 8:
        has_decal1 = br.read_bool()
        if has_decal1:
            td = read_tex_desc()
            info['slots']['Decal1'] = td
            print(f"  [7] Decal1: source={td['source_ref']}")
        else:
            print(f"  [7] Decal1: (none)")

    # Shader textures (version >= 10.0.1.0)
    info['num_shader_textures'] = br.read_uint32()
    for st_idx in range(info['num_shader_textures']):
        is_used = br.read_bool()
        if is_used:
            td = read_tex_desc()
            map_idx = br.read_uint32()
            print(f"  shader_tex[{st_idx}]: source={td['source_ref']}, map_idx={map_idx}")

    size = br.tell() - start
    print(f"  ({size} bytes)")
    return info, size


def parse_NiSourceTexture(br, idx):
    """NiSourceTexture"""
    start = br.tell()
    info = parse_ni_object_net(br)

    info['use_external'] = br.read_uint8()

    if info['use_external']:
        info['file_name'] = br.read_string()
        info['unknown_link'] = br.read_ref()  # version >= 10.1.0.0
    else:
        # Internal texture
        info['file_name'] = br.read_string()  # version >= 10.1.0.0
        info['pixel_data_ref'] = br.read_ref()

    info['pixel_layout'] = br.read_uint32()
    info['use_mipmaps'] = br.read_uint32()
    info['alpha_format'] = br.read_uint32()
    info['is_static'] = br.read_uint8()

    # Direct Render (version >= 10.1.0.106)
    info['direct_render'] = br.read_bool()

    size = br.tell() - start

    pix_layouts = {0:'PAL8', 1:'HIGH_COLOR_16', 2:'TRUE_COLOR_32', 3:'COMPRESSED', 4:'BUMPMAP', 5:'PAL4', 6:'DEFAULT'}
    mip_fmts = {0:'NO', 1:'YES', 2:'DEFAULT'}
    alpha_fmts = {0:'NONE', 1:'BINARY', 2:'SMOOTH', 3:'DEFAULT'}

    print(f"\n  --- NiSourceTexture [block {idx}] ({size} bytes) ---")
    print(f"  name='{info['name']}', use_external={info['use_external']}")
    print(f"  file_name='{info.get('file_name', '')}'")
    print(f"  pixel_layout={pix_layouts.get(info['pixel_layout'], info['pixel_layout'])}")
    print(f"  use_mipmaps={mip_fmts.get(info['use_mipmaps'], info['use_mipmaps'])}")
    print(f"  alpha_format={alpha_fmts.get(info['alpha_format'], info['alpha_format'])}")
    print(f"  is_static={info['is_static']}, direct_render={info['direct_render']}")
    return info, size


def parse_NiMaterialProperty(br, idx):
    """NiMaterialProperty: for v20.0.0.4 has NO flags field (flags only for ver 3.0 to 10.0.1.2)"""
    start = br.tell()
    info = parse_ni_object_net(br)

    # NO Flags for this version (ver2=10.0.1.2 means flags stop at 10.0.1.2)
    info['ambient'] = br.read_color3()
    info['diffuse'] = br.read_color3()
    info['specular'] = br.read_color3()
    info['emissive'] = br.read_color3()
    info['glossiness'] = br.read_float()
    info['alpha'] = br.read_float()

    size = br.tell() - start
    print(f"\n  --- NiMaterialProperty [block {idx}] ({size} bytes) ---")
    print(f"  name='{info['name']}'")
    print(f"  ambient=({info['ambient'][0]:.3f}, {info['ambient'][1]:.3f}, {info['ambient'][2]:.3f})")
    print(f"  diffuse=({info['diffuse'][0]:.3f}, {info['diffuse'][1]:.3f}, {info['diffuse'][2]:.3f})")
    print(f"  specular=({info['specular'][0]:.3f}, {info['specular'][1]:.3f}, {info['specular'][2]:.3f})")
    print(f"  emissive=({info['emissive'][0]:.3f}, {info['emissive'][1]:.3f}, {info['emissive'][2]:.3f})")
    print(f"  glossiness={info['glossiness']:.3f}, alpha={info['alpha']:.3f}")
    return info, size


def parse_NiAlphaProperty(br, idx):
    start = br.tell()
    info = parse_ni_object_net(br)
    info['flags'] = br.read_flags()
    info['threshold'] = br.read_uint8()
    size = br.tell() - start
    print(f"\n  --- NiAlphaProperty [block {idx}] ({size} bytes) ---")
    print(f"  flags=0x{info['flags']:04X}, threshold={info['threshold']}")
    print(f"    blend_enable={bool(info['flags']&1)}, src_blend={(info['flags']>>1)&0xF}, dst_blend={(info['flags']>>5)&0xF}")
    print(f"    test_enable={bool(info['flags']&(1<<9))}, test_func={(info['flags']>>10)&0x7}")
    return info, size


def parse_NiZBufferProperty(br, idx):
    start = br.tell()
    info = parse_ni_object_net(br)
    info['flags'] = br.read_flags()
    # Function: present for versions 4.1.0.12 to 20.0.0.5 (includes us)
    info['function'] = br.read_uint32()
    size = br.tell() - start
    print(f"\n  --- NiZBufferProperty [block {idx}] ({size} bytes) ---")
    print(f"  flags=0x{info['flags']:04X} (z_test={bool(info['flags']&1)}, z_write={bool(info['flags']&2)})")
    print(f"  function={info['function']}")
    return info, size


def parse_NiVertexColorProperty(br, idx):
    start = br.tell()
    info = parse_ni_object_net(br)
    info['flags'] = br.read_flags()
    # Vertex Mode and Lighting Mode: present for versions <= 20.0.0.5
    info['vertex_mode'] = br.read_uint32()
    info['lighting_mode'] = br.read_uint32()
    size = br.tell() - start
    print(f"\n  --- NiVertexColorProperty [block {idx}] ({size} bytes) ---")
    print(f"  flags=0x{info['flags']:04X}")
    print(f"  vertex_mode={info['vertex_mode']} (0=IGNORE, 1=EMISSIVE, 2=AMB_DIF)")
    print(f"  lighting_mode={info['lighting_mode']} (0=E, 1=E_A_D)")
    return info, size


def parse_NiStencilProperty(br, idx):
    start = br.tell()
    info = parse_ni_object_net(br)
    # Flags only for ver <= 10.0.1.2, NOT present for us
    # Fields for ver <= 20.0.0.5:
    info['stencil_enabled'] = br.read_uint8()
    info['stencil_function'] = br.read_uint32()
    info['stencil_ref'] = br.read_uint32()
    info['stencil_mask'] = br.read_uint32()
    info['fail_action'] = br.read_uint32()
    info['z_fail_action'] = br.read_uint32()
    info['pass_action'] = br.read_uint32()
    info['draw_mode'] = br.read_uint32()
    size = br.tell() - start
    print(f"\n  --- NiStencilProperty [block {idx}] ({size} bytes) ---")
    print(f"  stencil_enabled={info['stencil_enabled']}, draw_mode={info['draw_mode']}")
    return info, size


def parse_NiSpecularProperty(br, idx):
    start = br.tell()
    info = parse_ni_object_net(br)
    info['flags'] = br.read_flags()
    size = br.tell() - start
    print(f"\n  --- NiSpecularProperty [block {idx}] ({size} bytes) ---")
    print(f"  flags=0x{info['flags']:04X}")
    return info, size


def parse_NiStringExtraData(br, idx):
    start = br.tell()
    # NiExtraData: just name from NiObject
    name = br.read_string()
    value = br.read_string()
    size = br.tell() - start
    print(f"\n  --- NiStringExtraData [block {idx}] ({size} bytes) ---")
    print(f"  name='{name}', value='{value}'")
    return {'name': name, 'value': value}, size


def parse_NiIntegerExtraData(br, idx):
    start = br.tell()
    name = br.read_string()
    value = br.read_uint32()
    size = br.tell() - start
    print(f"\n  --- NiIntegerExtraData [block {idx}] ({size} bytes) ---")
    print(f"  name='{name}', value={value}")
    return {'name': name, 'value': value}, size


def parse_NiDynamicEffect_common(br):
    """Common fields for NiDynamicEffect (NiAVObject + switch state + affected nodes)"""
    info = parse_ni_av_object(br)
    # Switch State (version >= 10.1.0.106)
    info['switch_state'] = br.read_bool()
    # Num Affected Nodes (version >= 10.1.0.0)
    num_affected = br.read_uint32()
    info['num_affected_nodes'] = num_affected
    info['affected_node_refs'] = [br.read_ref() for _ in range(num_affected)]
    return info


def parse_NiLight_common(br):
    """NiLight: NiDynamicEffect + dimmer + ambient + diffuse + specular colors"""
    info = parse_NiDynamicEffect_common(br)
    info['dimmer'] = br.read_float()
    info['ambient_color'] = br.read_color3()
    info['diffuse_color'] = br.read_color3()
    info['specular_color'] = br.read_color3()
    return info


def parse_NiPointLight(br, idx):
    start = br.tell()
    info = parse_NiLight_common(br)
    info['constant_attenuation'] = br.read_float()
    info['linear_attenuation'] = br.read_float()
    info['quadratic_attenuation'] = br.read_float()
    size = br.tell() - start
    print(f"\n  --- NiPointLight [block {idx}] ({size} bytes) ---")
    print(f"  name='{info['name']}', dimmer={info['dimmer']:.3f}")
    return info, size


def parse_NiDirectionalLight(br, idx):
    start = br.tell()
    info = parse_NiLight_common(br)
    size = br.tell() - start
    print(f"\n  --- NiDirectionalLight [block {idx}] ({size} bytes) ---")
    print(f"  name='{info['name']}', dimmer={info['dimmer']:.3f}")
    return info, size


def parse_NiAmbientLight(br, idx):
    start = br.tell()
    info = parse_NiLight_common(br)
    size = br.tell() - start
    print(f"\n  --- NiAmbientLight [block {idx}] ({size} bytes) ---")
    print(f"  name='{info['name']}', dimmer={info['dimmer']:.3f}")
    return info, size


def parse_NiTimeController_common(br):
    """NiTimeController fields"""
    info = {}
    info['next_controller_ref'] = br.read_ref()
    info['flags'] = br.read_flags()
    info['frequency'] = br.read_float()
    info['phase'] = br.read_float()
    info['start_time'] = br.read_float()
    info['stop_time'] = br.read_float()
    info['target_ref'] = br.read_ref()
    return info


def parse_NiSingleInterpController_common(br):
    """NiSingleInterpController: NiTimeController + interpolator ref (version >= 10.1.0.104)"""
    info = parse_NiTimeController_common(br)
    info['interpolator_ref'] = br.read_ref()
    return info


def parse_NiTransformController(br, idx):
    start = br.tell()
    info = parse_NiSingleInterpController_common(br)
    size = br.tell() - start
    print(f"\n  --- NiTransformController [block {idx}] ({size} bytes) ---")
    print(f"  target={info['target_ref']}, interp={info['interpolator_ref']}")
    print(f"  flags=0x{info['flags']:04X}, freq={info['frequency']:.2f}, time=[{info['start_time']:.2f}, {info['stop_time']:.2f}]")
    return info, size


def parse_NiTransformInterpolator(br, idx):
    """NiTransformInterpolator: NiKeyBasedInterpolator -> NiInterpolator -> NiObject"""
    start = br.tell()
    info = {}
    # Translation (vec3)
    info['translation'] = br.read_vec3()
    # Rotation (quaternion = 4 floats)
    info['rotation'] = (br.read_float(), br.read_float(), br.read_float(), br.read_float())
    # Scale
    info['scale'] = br.read_float()
    # Data ref
    info['data_ref'] = br.read_ref()
    size = br.tell() - start
    print(f"\n  --- NiTransformInterpolator [block {idx}] ({size} bytes) ---")
    t = info['translation']
    print(f"  translation=({t[0]:.4f}, {t[1]:.4f}, {t[2]:.4f}), scale={info['scale']:.4f}")
    print(f"  data_ref={info['data_ref']}")
    return info, size


def parse_NiKeyframeData(br, idx):
    """NiKeyframeData (also NiTransformData which inherits but adds nothing)"""
    start = br.tell()
    info = {}

    # Rotation keys
    num_rot_keys = br.read_uint32()
    info['num_rotation_keys'] = num_rot_keys
    if num_rot_keys > 0:
        rot_type = br.read_uint32()  # KeyType
        info['rotation_type'] = rot_type
        # Type 1 = LINEAR: time + quaternion
        # Type 2 = QUADRATIC: time + quaternion + tangents
        # Type 3 = TBC: time + quaternion + TBC
        # Type 4 = XYZ_ROTATION: separate per-axis keys
        if rot_type == 4:
            # XYZ rotation: 3 sub-key arrays (one per axis)
            # Actually in version > 10.1.0.0, the time field is absent from the QuatKey
            for axis in range(3):
                n_axis_keys = br.read_uint32()
                if n_axis_keys > 0:
                    axis_type = br.read_uint32()
                    for _ in range(n_axis_keys):
                        br.read_float()  # time
                        br.read_float()  # value
                        if axis_type == 2:
                            br.read_float()  # forward
                            br.read_float()  # backward
                        elif axis_type == 3:
                            br.read_float()  # t
                            br.read_float()  # b
                            br.read_float()  # c
        else:
            for _ in range(num_rot_keys):
                br.read_float()  # time
                if rot_type != 4:
                    # quaternion: w, x, y, z
                    br.read_float()
                    br.read_float()
                    br.read_float()
                    br.read_float()
                if rot_type == 3:
                    br.read_float()  # t
                    br.read_float()  # b
                    br.read_float()  # c

    # Translation keys (KeyGroup<Vector3>)
    num_trans_keys = br.read_uint32()
    info['num_translation_keys'] = num_trans_keys
    if num_trans_keys > 0:
        trans_type = br.read_uint32()
        for _ in range(num_trans_keys):
            br.read_float()  # time
            br.read_float(); br.read_float(); br.read_float()  # value (vec3)
            if trans_type == 2:
                br.read_float(); br.read_float(); br.read_float()  # forward
                br.read_float(); br.read_float(); br.read_float()  # backward
            elif trans_type == 3:
                br.read_float(); br.read_float(); br.read_float()  # TBC

    # Scale keys (KeyGroup<float>)
    num_scale_keys = br.read_uint32()
    info['num_scale_keys'] = num_scale_keys
    if num_scale_keys > 0:
        scale_type = br.read_uint32()
        for _ in range(num_scale_keys):
            br.read_float()  # time
            br.read_float()  # value
            if scale_type == 2:
                br.read_float()  # forward
                br.read_float()  # backward
            elif scale_type == 3:
                br.read_float(); br.read_float(); br.read_float()  # TBC

    size = br.tell() - start
    print(f"\n  --- NiKeyframeData/NiTransformData [block {idx}] ({size} bytes) ---")
    print(f"  rot_keys={num_rot_keys}, trans_keys={num_trans_keys}, scale_keys={num_scale_keys}")
    return info, size


# Alias
parse_NiTransformData = parse_NiKeyframeData


# ============================================================================
# PARSER DISPATCH TABLE
# ============================================================================

PARSERS = {
    'NiNode': parse_NiNode,
    'NiTriShape': parse_NiTriShape,
    'NiTriStrips': parse_NiTriStrips,
    'NiTriShapeData': parse_NiTriShapeData,
    'NiTriStripsData': parse_NiTriStripsData,
    'NiTexturingProperty': parse_NiTexturingProperty,
    'NiSourceTexture': parse_NiSourceTexture,
    'NiMaterialProperty': parse_NiMaterialProperty,
    'NiAlphaProperty': parse_NiAlphaProperty,
    'NiZBufferProperty': parse_NiZBufferProperty,
    'NiVertexColorProperty': parse_NiVertexColorProperty,
    'NiStencilProperty': parse_NiStencilProperty,
    'NiSpecularProperty': parse_NiSpecularProperty,
    'NiStringExtraData': parse_NiStringExtraData,
    'NiIntegerExtraData': parse_NiIntegerExtraData,
    'NiPointLight': parse_NiPointLight,
    'NiDirectionalLight': parse_NiDirectionalLight,
    'NiAmbientLight': parse_NiAmbientLight,
    'NiTransformController': parse_NiTransformController,
    'NiTransformInterpolator': parse_NiTransformInterpolator,
    'NiTransformData': parse_NiTransformData,
}


# ============================================================================
# MAIN
# ============================================================================

def parse_nif(filepath, max_blocks=None):
    file_size = os.path.getsize(filepath)
    print(f"\n{'#' * 80}")
    print(f"# PARSING: {os.path.basename(filepath)}")
    print(f"# File size: {file_size} bytes")
    print(f"{'#' * 80}")

    with open(filepath, 'rb') as f:
        br = BinaryReader(f)
        hdr = parse_header(br)
        print_header(hdr)

        block_data_start = br.tell()
        print(f"Block data starts at offset: {block_data_start} (0x{block_data_start:X})")

        print("\n" + "=" * 80)
        print("BLOCK DETAILS")
        print("=" * 80)

        blocks_parsed = 0
        total_block_bytes = 0

        for i in range(hdr.num_blocks):
            type_idx = hdr.block_type_indices[i]
            type_name = hdr.block_type_names[type_idx]
            block_start = br.tell()

            if max_blocks is not None and blocks_parsed >= max_blocks:
                # Can't skip without knowing block size - must parse all
                pass

            if type_name in PARSERS:
                try:
                    info, size = PARSERS[type_name](br, i)
                    total_block_bytes += size
                    blocks_parsed += 1
                except Exception as e:
                    print(f"\n  *** ERROR parsing {type_name} [block {i}] at offset {block_start}: {e} ***")
                    import traceback
                    traceback.print_exc()
                    # Cannot recover without block sizes
                    break
            else:
                print(f"\n  *** UNKNOWN BLOCK TYPE: {type_name} [block {i}] at offset {block_start} ***")
                print(f"  Cannot skip - no block size array. Stopping parse.")
                break

        final_pos = br.tell()
        print(f"\n\nParsed {blocks_parsed} / {hdr.num_blocks} blocks")
        print(f"Final position: {final_pos} / {file_size}")
        if final_pos == file_size:
            print("SUCCESS: Consumed entire file!")
        else:
            remaining = file_size - final_pos
            print(f"Remaining: {remaining} bytes")
            if remaining > 0 and remaining < 100:
                leftover = br.read_bytes(remaining)
                print(f"Leftover hex: {leftover.hex(' ')}")

    return hdr


def print_format_summary():
    print("\n\n")
    print("=" * 80)
    print("NIF BINARY FORMAT REFERENCE FOR C++ PARSER")
    print("Version: 20.0.0.4 (0x14000004), Gamebryo, user_version=0")
    print("Target: Civilization IV: Beyond the Sword")
    print("Source: niftools nif.xml + empirical verification")
    print("=" * 80)

    print("""
HEADER LAYOUT
=============
  bytes     header_string;       // ASCII ending with 0x0A, e.g. "Gamebryo File Format, Version 20.0.0.4\\n"
  uint32    version;             // 0x14000004
  uint8     endian;              // 1 = little endian
  uint32    user_version;        // 0 for Civ4
  uint32    num_blocks;          // total block count
  // NOTE: NO user_version_2 (only if user_version >= 10)
  // NOTE: NO export_info strings (only if user_version >= 10)
  uint16    num_block_types;
  SizedString block_type_names[num_block_types];  // uint32 len + chars each
  uint16    block_type_index[num_blocks];          // type index per block
  // NOTE: NO block_size array (only version >= 20.2.0.7)
  // NOTE: NO string table (only version >= 20.1.0.3)
  uint32    unknown_int_2;       // always 0 (present for version >= 10.0.1.0)

  Block data follows immediately. Must parse sequentially.

PRIMITIVE TYPES
===============
  bool:         uint8 (1 byte), 0=false, nonzero=true (version >= 4.1.0.1)
  Flags:        uint16 (2 bytes)
  Ref:          int32 (4 bytes), -1 = null/none, otherwise 0-based block index
  string:       uint32 length + length ASCII chars (no null terminator, no string table)
  SizedString:  same as string
  FilePath:     same as string
  Vector3:      3x float (12 bytes)
  Matrix33:     9x float (36 bytes), row-major
  Quaternion:   4x float (w, x, y, z) (16 bytes)
  Color3:       3x float RGB (12 bytes)
  Color4:       4x float RGBA (16 bytes)
  TexCoord:     2x float (u, v) (8 bytes)
  Triangle:     3x uint16 (v0, v1, v2) (6 bytes)

INHERITANCE CHAINS
==================
  NiObject (empty base)
    NiObjectNET:
      string    name;
      uint32    num_extra_data;
      Ref       extra_data_refs[num_extra_data];
      Ref       controller_ref;

    NiAVObject (extends NiObjectNET):
      Flags     flags;              // uint16
      Vector3   translation;        // 3 floats
      Matrix33  rotation;           // 9 floats
      float     scale;
      uint32    num_properties;
      Ref       property_refs[num_properties];
      Ref       collision_ref;

    NiGeometry (extends NiAVObject):
      Ref       data_ref;           // -> NiTriShapeData or NiTriStripsData
      Ref       skin_instance_ref;  // -> NiSkinInstance, -1 = none
      bool      has_shader;         // (version 10.0.1.0 to 20.1.0.3)
      if has_shader:
        string  shader_name;
        int32   shader_unknown;

TARGET BLOCK TYPES
==================

NiNode (extends NiAVObject):
  [NiAVObject fields]
  uint32    num_children;
  Ref       children[num_children];     // -1 = empty slot
  uint32    num_effects;
  Ref       effects[num_effects];

NiTriShape / NiTriStrips (extends NiGeometry -> NiAVObject):
  [NiAVObject fields]
  Ref       data_ref;                   // -> NiTriShapeData or NiTriStripsData
  Ref       skin_instance_ref;
  bool      has_shader;
  if has_shader:
    string  shader_name;
    int32   shader_unknown;

NiTriShapeData (extends NiGeometryData):
  int32     group_id;                    // usually 0
  uint16    num_vertices;
  uint8     keep_flags;
  uint8     compress_flags;
  bool      has_vertices;                // 1 byte
  if has_vertices:
    Vector3 vertices[num_vertices];      // 12 bytes each
  uint8     num_uv_sets;                 // byte! (bits 0-5 = count, NOT uint16)
  uint8     extra_vectors_flags;         // bit 4 = has tangents/bitangents
  bool      has_normals;
  if has_normals:
    Vector3 normals[num_vertices];
  if has_normals AND (extra_vectors_flags & 0x10):
    Vector3 tangents[num_vertices];
    Vector3 bitangents[num_vertices];
  Vector3   center;                      // bounding sphere center
  float     radius;                      // bounding sphere radius
  bool      has_vertex_colors;
  if has_vertex_colors:
    Color4  colors[num_vertices];        // 16 bytes each
  // UV data (num_uv_sets & 0x3F) sets, each has num_vertices TexCoords:
  for each uv_set:
    TexCoord uv[num_vertices];           // 8 bytes each
  uint16    consistency_flags;
  Ref       additional_data_ref;
  // NiTriBasedGeomData:
  uint16    num_triangles;
  // NiTriShapeData-specific:
  uint32    num_triangle_points;         // = num_triangles * 3
  bool      has_triangles;
  if has_triangles:
    Triangle triangles[num_triangles];   // 6 bytes each
  uint16    num_match_groups;
  for each match_group:
    uint16  count;
    uint16  indices[count];

NiTriStripsData (extends NiGeometryData):
  [same NiGeometryData fields as above]
  // NiTriBasedGeomData:
  uint16    num_triangles;
  // NiTriStripsData-specific:
  uint16    num_strips;
  uint16    strip_lengths[num_strips];
  bool      has_points;
  if has_points:
    for each strip s:
      uint16 points[strip_lengths[s]];

NiTexturingProperty (extends NiProperty -> NiObjectNET):
  [NiObjectNET fields]
  uint32    apply_mode;                  // (version <= 20.0.0.5) 0=REPLACE, 1=DECAL, 2=MODULATE, 3=HILIGHT, 4=HILIGHT2
  uint32    texture_count;               // 7 or 8
  // Texture slots (each: bool has_tex, then TexDesc if true):
  bool      has_base_texture;
  TexDesc?  base_texture;                // slot 0: diffuse
  bool      has_dark_texture;
  TexDesc?  dark_texture;                // slot 1
  bool      has_detail_texture;
  TexDesc?  detail_texture;              // slot 2
  bool      has_gloss_texture;
  TexDesc?  gloss_texture;               // slot 3
  bool      has_glow_texture;
  TexDesc?  glow_texture;                // slot 4: emissive
  bool      has_bump_map_texture;
  TexDesc?  bump_map_texture;            // slot 5
  if has_bump_map_texture:
    float   bump_luma_scale;
    float   bump_luma_offset;
    float   bump_matrix[4];              // 2x2 matrix
  bool      has_decal_0_texture;         // slot 6 (if texture_count >= 7)
  TexDesc?  decal_0_texture;
  bool      has_decal_1_texture;         // (if texture_count >= 8, ver <= 20.1.0.3)
  TexDesc?  decal_1_texture;
  uint32    num_shader_textures;
  ShaderTexDesc shader_textures[num_shader_textures];

  TexDesc compound:
    Ref       source_ref;                // -> NiSourceTexture
    uint32    clamp_mode;                // TexClampMode: 0=CLAMP_S_CLAMP_T, 1=CLAMP_S_WRAP_T, 2=WRAP_S_CLAMP_T, 3=WRAP_S_WRAP_T
    uint32    filter_mode;               // TexFilterMode: 0=NEAREST, 1=BILERP, 2=TRILERP
    uint32    uv_set;                    // which UV set index
    bool      has_texture_transform;
    if has_texture_transform:
      float   translation[2];            // UV offset
      float   tiling[2];                 // UV scale
      float   w_rotation;               // UV rotation
      uint32  transform_type;           // 0=MAYA_LEGACY, 1=MAX, 2=MAYA
      float   center_offset[2];         // transform center

  ShaderTexDesc compound:
    bool      is_used;
    if is_used:
      TexDesc texture_data;
      uint32  map_index;

NiSourceTexture (extends NiTexture -> NiObjectNET):
  [NiObjectNET fields]
  uint8     use_external;                // 1 = external file, 0 = embedded
  if use_external:
    string  file_name;                   // texture file path
    Ref     unknown_link;                // usually -1
  else:
    string  file_name;                   // original filename
    Ref     pixel_data_ref;              // -> NiPixelData
  uint32    pixel_layout;                // 6=DEFAULT
  uint32    use_mipmaps;                 // 2=DEFAULT
  uint32    alpha_format;                // 3=DEFAULT
  uint8     is_static;                   // usually 1
  bool      direct_render;               // (version >= 10.1.0.106) usually 1

NiMaterialProperty (extends NiProperty -> NiObjectNET):
  [NiObjectNET fields]
  // NOTE: NO Flags field for version 20.0.0.4 (only versions 3.0 to 10.0.1.2)
  Color3    ambient;                     // 3 floats
  Color3    diffuse;                     // 3 floats
  Color3    specular;                    // 3 floats
  Color3    emissive;                    // 3 floats
  float     glossiness;                  // 0-128
  float     alpha;                       // 0=transparent, 1=opaque

NiAlphaProperty:
  [NiObjectNET fields]
  Flags     flags;                       // packed blend/test modes (see above)
  uint8     threshold;                   // alpha test threshold 0-255

NiZBufferProperty:
  [NiObjectNET fields]
  Flags     flags;                       // bit 0=z_test, bit 1=z_write
  uint32    function;                    // depth compare mode

NiVertexColorProperty:
  [NiObjectNET fields]
  Flags     flags;
  uint32    vertex_mode;                 // 0=IGNORE, 1=EMISSIVE, 2=AMB_DIF
  uint32    lighting_mode;               // 0=E, 1=E_A_D

NiStencilProperty:
  [NiObjectNET fields]
  // NO Flags for version 20.0.0.4 (only ver <= 10.0.1.2)
  uint8     stencil_enabled;
  uint32    stencil_function;
  uint32    stencil_ref;
  uint32    stencil_mask;
  uint32    fail_action;
  uint32    z_fail_action;
  uint32    pass_action;
  uint32    draw_mode;                   // 0=CCW_OR_BOTH, 1=CCW, 2=CW, 3=BOTH

FOOTER (after all blocks)
=========================
  uint32    num_roots;                   // usually 1
  Ref       roots[num_roots];            // block indices of root objects (usually [0])

SCENE GRAPH STRUCTURE
=====================
Block 0 is always the root NiNode ("Scene Root").
NiNode.children[] contains refs to child NiNodes and NiTriShape/NiTriStrips.
NiTriShape.data_ref points to a NiTriShapeData block.
NiTriShape.property_refs point to NiMaterialProperty, NiTexturingProperty, etc.
NiTexturingProperty contains TexDescs that point to NiSourceTexture blocks.
NiSourceTexture.file_name gives the texture file path.

NOTE ON ENUM TYPES: Most enums (TexClampMode, TexFilterMode, ApplyMode,
VertMode, LightMode, ZCompareMode, StencilCompareMode, StencilAction,
FaceDrawMode) are stored as uint32 (4 bytes). Exception: ConsistencyType
is stored as uint16 (2 bytes). Flags are always uint16 (2 bytes).

VERIFIED AGAINST:
  - throneRoom-geometryOnly.nif (168339 bytes, 146 blocks) - ALL BLOCKS PARSED
  - WaterEnvironment.nif (26134 bytes, 45 blocks) - 9 blocks parsed (stopped at NiTextureTransformController)
""")


if __name__ == '__main__':
    candidates = [
        r"C:\Program Files (x86)\Steam\steamapps\common\Sid Meier's Civilization IV Beyond the Sword\Assets\Art\Interface\Screens\Throne\throneRoom-geometryOnly.nif",
        r"C:\Program Files (x86)\Steam\steamapps\common\Sid Meier's Civilization IV Beyond the Sword\Assets\Art\Interface\Screens\Civilopedia\WaterEnvironment\WaterEnvironment.nif",
        r"C:\Program Files (x86)\Steam\steamapps\common\Sid Meier's Civilization IV Beyond the Sword\Assets\Art\Interface\Screens\SpaceShip\LaunchPad.nif",
        r"C:\Program Files (x86)\Steam\steamapps\common\Sid Meier's Civilization IV Beyond the Sword\Assets\Art\LeaderHeads\Abraham Lincoln\Abraham Lincoln.nif",
    ]

    parsed_files = []

    for filepath in candidates:
        if os.path.exists(filepath):
            try:
                hdr = parse_nif(filepath)
                parsed_files.append(filepath)
                if len(parsed_files) >= 2:
                    break  # Parse 2 files for validation
            except Exception as e:
                import traceback
                print(f"\n*** ERROR: {e} ***")
                traceback.print_exc()
                continue

    if not parsed_files:
        print("No files could be parsed!")

    print_format_summary()
