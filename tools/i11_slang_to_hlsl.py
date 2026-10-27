"""Stage I11.4 (DECISIONS.md T-I11-4): turns the libretro shaders of the "crt-guest-advanced-ntsc" preset (Slang, i.e. Vulkan GLSL) into HLSL for Direct3D 11.

    python -I tools/i11_slang_to_hlsl.py [--check <folder>] [--out <file>]

It reads third_party/shaders/libretro/crt/crt-guest-advanced-ntsc.slangp and the shaders it names (data, never edited) and writes
src/host/gdv_generated.cpp (all the generated text and tables). With --check <folder> it also writes each pass as vertex/pixel .hlsl files
in that folder, to be compiled with fxc.exe. The translation is mechanical (types, functions, the way textures are read, constants); the few
places where HLSL and GLSL differ in meaning (the order of matrix products) are patched by hand in PATCHES below, each with the reason.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CRT = os.path.join(ROOT, 'third_party', 'shaders', 'libretro', 'crt')
PRESET = os.path.join(CRT, 'crt-guest-advanced-ntsc.slangp')

# Exact text replacements done on a shader after the mechanical translation: file stem -> list of (old, new, why).
MATRIX = ('the matrix is built with the same order of numbers in both languages, so row i of the HLSL matrix is column i of the GLSL one; '
          'GLSL vector * matrix is then mul(matrix, vector) and matrix * vector is mul(vector, matrix)')
YIQ = [('return yiq * yiq2rgb_mat;', 'return mul(yiq2rgb_mat, yiq);', MATRIX), ('return col * yiq_mat;', 'return mul(yiq_mat, col);', MATRIX)]
PATCHES = {   # pixel stage only
    'ntsc-pass1': YIQ + [('yiq2 *= mix_mat;', 'yiq2 = mul(mix_mat, yiq2);', MATRIX), ('yiq *= mix_mat;', 'yiq = mul(mix_mat, yiq);', MATRIX)],
    'ntsc-pass2': YIQ,
    'ntsc-pass3': YIQ,
    'custom-fast-sharpen': YIQ,
    'pre-shaders-afterglow': [('color = m_in*color;', 'color = mul(color, m_in);', MATRIX), ('color = m_out*color;', 'color = mul(color, m_out);', MATRIX),
                              ('warmer = D65_to_D55*color;', 'warmer = mul(color, D65_to_D55);', MATRIX), ('warmer = ToSRGB*warmer;', 'warmer = mul(warmer, ToSRGB);', MATRIX),
                              ('cooler = D65_to_D93*color;', 'cooler = mul(color, D65_to_D93);', MATRIX), ('cooler = ToSRGB*cooler;', 'cooler = mul(cooler, ToSRGB);', MATRIX)],
}


def read(path):
    with open(path, encoding='utf-8', errors='replace', newline='') as f:
        return f.read().replace('\r\n', '\n')


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    return re.sub(r'//[^\n]*', '', s)


def call_args(s, open_idx):
    """s[open_idx] is '('. Returns (list of argument texts, index after the closing ')')."""
    depth = 0
    args, cur = [], []
    i = open_idx
    while i < len(s):
        c = s[i]
        if c in '([{':
            depth += 1
            if depth > 1:
                cur.append(c)
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                args.append(''.join(cur).strip())
                return args, i + 1
            cur.append(c)
        elif c == ',' and depth == 1:
            args.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    raise ValueError('unbalanced parentheses')


def rewrite_calls(s, pattern, fn):
    """Calls fn(match_text, args) for every call of pattern( ... ), from the last to the first; fn returns the replacement or None."""
    found = [m for m in re.finditer(pattern, s)]
    for m in reversed(found):
        open_idx = s.index('(', m.start())
        args, end = call_args(s, open_idx)
        rep = fn(m.group(0), args)
        if rep is not None:
            s = s[:m.start()] + rep + s[end:]
    return s


VEC = {'vec2': 'float2', 'vec3': 'float3', 'vec4': 'float4', 'ivec2': 'int2', 'ivec3': 'int3', 'ivec4': 'int4', 'uvec2': 'uint2',
       'mat2': 'float2x2', 'mat3': 'float3x3', 'mat4': 'float4x4', 'bvec2': 'bool2', 'bvec3': 'bool3', 'bvec4': 'bool4'}
RENAME = {'mix': 'lerp', 'fract': 'frac', 'inversesqrt': 'rsqrt', 'sample': 'sample_', 'line': 'line_', 'point': 'point_', 'triangle': 'triangle_',
          'linear': 'linear_', 'input': 'input_', 'output': 'output_', 'texture2D': 'texture2D_', 'half': 'half_', 'fixed': 'fixed_', 'precise': 'precise_',
          'interface': 'interface_', 'pass': 'pass_', 'vector': 'vector_', 'matrix': 'matrix_', 'register': 'register_', 'packed': 'packed_',
          'sampler': 'sampler_', 'Texture': 'Texture_', 'texture': 'texture'}

PRELUDE = '''float mod(float x, float y) { return x - y * floor(x / y); }
float2 mod(float2 x, float2 y) { return x - y * floor(x / y); }
float3 mod(float3 x, float3 y) { return x - y * floor(x / y); }
float4 mod(float4 x, float4 y) { return x - y * floor(x / y); }
float2 mod(float2 x, float y) { return x - y * floor(x / y); }
float3 mod(float3 x, float y) { return x - y * floor(x / y); }
float4 mod(float4 x, float y) { return x - y * floor(x / y); }
'''


def translate_code(s, textures):
    """GLSL text (comments gone) to HLSL text."""
    s = rewrite_calls(s, r'\bfloat\s*\[\s*\d*\s*\]\s*\(', lambda t, a: '{ ' + ', '.join(a) + ' }')   # float[3](a, b, c) is {a, b, c}

    def one_arg(name, args):
        if name.startswith('vec') and len(args) == 1:
            return '((%s)(%s))' % (VEC[name.rstrip('(').strip()], args[0])
        return None
    s = rewrite_calls(s, r'\bvec[234]\s*\(', lambda t, a: one_arg(t.rstrip('( ').strip(), a) if len(a) == 1 else None)
    s = rewrite_calls(s, r'\bivec[234]\s*\(', lambda t, a: '((%s)(%s))' % (VEC[t.rstrip('( ').strip()], a[0]) if len(a) == 1 else None)
    s = rewrite_calls(s, r'\bmat[234]\s*\(', lambda t, a: '((%s)(%s))' % (VEC[t.rstrip('( ').strip()], a[0]) if len(a) == 1 else None)

    def tex(name, args):
        if len(args) < 2 or not re.fullmatch(r'\w+', args[0]):
            raise ValueError('texture call with an unknown sampler: %s %s' % (name, args))
        if args[0] not in textures:   # inside a #define: the sampler is the macro's own argument; its state is found by pasting `_s` onto the name
            return '%s.SampleLevel(%s##_s, %s, %s)' % (args[0], args[0], args[1], args[2] if name.startswith('textureLod') else '0.0')
        lod = '0.0'
        if name.startswith('textureLod'):
            lod = args[2]
        elif len(args) > 2:
            raise ValueError('texture() with a bias is not handled: %s' % args)
        return '%s.SampleLevel(%s_s, %s, %s)' % (args[0], args[0], args[1], lod)
    s = rewrite_calls(s, r'\btextureLod\s*\(', tex)
    s = rewrite_calls(s, r'\btexture\s*\(', tex)
    s = rewrite_calls(s, r'\btextureSize\s*\(', lambda t, a: '%s_size()' % a[0])
    for k, v in VEC.items():
        s = re.sub(r'\b%s\b' % k, v, s)
    for k, v in RENAME.items():
        if k != v:
            s = re.sub(r'(?<![\w.])%s\b' % k, v, s)
    s = re.sub(r'(?<![\w.])(\d+\.\d+)\.([xyzwrgba]{1,4})\b', r'((float)\1).\2', s)
    return s


def inline_self_macros(s):
    """`#define X params.X` (a macro that names itself) is fine in C but not for the Direct3D preprocessor: the macro is taken out and X is replaced in the text
    by what it stood for, exactly as the C preprocessor would have (no extra parentheses)."""
    for m in list(re.finditer(r'^[ \t]*#define[ \t]+(\w+)[ \t]+([^\n]*?)[ \t]*$', s, flags=re.M)):
        name, body = m.group(1), m.group(2)
        if not re.search(r'\.%s\b' % name, body):
            continue
        s = s.replace(m.group(0), '', 1)
        s = re.sub(r'(?<![\w.])%s\b' % name, lambda _m, b=body: b, s)
    return s


def static_globals(s):
    """Variables at file level get `static` (and the HLSL way to say const). A variable that is not a constant and whose first value is worked out from other things (the
    inputs of the stage, the settings) is declared on its own and given its value at the start of the stage's body: GLSL works such a value out when the stage starts,
    HLSL would do it before the inputs are in."""
    out, inits, depth = [], [], 0
    number = re.compile(r'(?<![\w.])\d+\.?\d*(e[+-]?\d+)?f?|\.\d+(e[+-]?\d+)?f?', re.I)
    for line in s.split('\n'):
        stripped = line.strip()
        if depth == 0 and not stripped.startswith('#') and re.match(
                r'^(const\s+)?(float|int|uint|bool|float[234]|int[234]|float[234]x[234])\s+\w+(\s*\[[^\]]*\])?\s*(=|;|,)', stripped):
            m =re.match(r'^(float|int|uint|bool|float[234]|int[234]|float[234]x[234])\s+(\w+)\s*=\s*(.*);$', stripped)
            if m and re.search(r'[A-Za-z_]', number.sub('', m.group(3))):   # not const, one line, worked out from something
                inits.append('  %s = %s;' % (m.group(2), m.group(3)))
                line = 'static %s %s;' % (m.group(1), m.group(2))
            else:
                line = 'static ' + line
        elif depth == 0 and re.match(r'^(const\s+)?float[234]x[234]\s+\w+\s*=\s*$', stripped):   # a matrix whose value starts on the next lines
            line = 'static ' + line
        depth += line.count('{') - line.count('}')
        out.append(line)
    return '\n'.join(out), inits


def parse_layout_decls(text):
    """Takes the `layout(...) in/out/uniform` declarations out of the text. Returns (text, ins, outs, samplers)."""
    ins, outs, samplers = [], [], []

    def decl(m):
        kind, typ, name = m.group(2), m.group(3), m.group(4)
        loc = re.search(r'location\s*=\s*(\d+)', m.group(1))
        if kind == 'in':
            ins.append((int(loc.group(1)), typ, name))
        else:
            outs.append((int(loc.group(1)), typ, name))
        return ''
    text = re.sub(r'layout\s*\(([^)]*location[^)]*)\)\s*(in|out)\s+(\w+)\s+(\w+)\s*;', decl, text)

    def sampler(m):
        samplers.append((int(re.search(r'binding\s*=\s*(\d+)', m.group(1)).group(1)), m.group(2)))
        return ''
    text = re.sub(r'layout\s*\(([^)]*binding[^)]*)\)\s*uniform\s+sampler2D\s+(\w+)\s*;', sampler, text)
    return text, ins, outs, samplers


HLSL_TYPE = {'float': 'float', 'vec2': 'float2', 'vec3': 'float3', 'vec4': 'float4', 'uint': 'uint', 'int': 'int'}


def translate_pass(path):
    src = read(path)
    stem = os.path.splitext(os.path.basename(path))[0]
    src = re.sub(r'^\s*#version[^\n]*\n', '', src, flags=re.M)
    src = re.sub(r'^\s*#extension[^\n]*\n', '', src, flags=re.M)
    iv, i_f = src.index('#pragma stage vertex'), src.index('#pragma stage fragment')
    assert iv < i_f
    common, vert, frag = src[:iv], src[iv:i_f], src[i_f:]

    params = []   # (name, default)
    for m in re.finditer(r'^\s*#pragma\s+parameter\s+(\w+)\s+"[^"]*"\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)', common, flags=re.M):
        params.append((m.group(1), float(m.group(2))))
    common = re.sub(r'^\s*#pragma\s+(parameter|name|format)[^\n]*\n', '', common, flags=re.M)

    def block(pattern, what):
        m = re.search(pattern, common, flags=re.S)
        assert m, (path, what)
        found = []
        for decl in strip_comments(m.group(1)).split(';'):
            decl = decl.strip()
            if not decl:
                continue
            typ, names = decl.split(None, 1)
            for n in names.split(','):
                found.append((typ, n.strip()))
        return m.group(0), found

    def layout_of(found):
        """Byte offsets the way Direct3D packs a constant buffer: a float4 starts a new register, scalars follow each other."""
        out, pos = [], 0
        for typ, n in found:
            assert typ in ('vec4', 'uint', 'float', 'int'), (path, typ, n)
            if typ == 'vec4':
                pos = (pos + 15) // 16 * 16
            out.append((typ, n, pos))
            pos += 16 if typ == 'vec4' else 4
        return out, (pos + 15) // 16 * 16

    def struct_text(name, laid):
        return 'struct %s {\n%s\n};\n' % (name, '\n'.join('  %s %s;' % (HLSL_TYPE[t], n) for t, n, _ in laid))

    text_push, members = block(r'layout\s*\(\s*push_constant\s*\)\s*uniform\s+Push\s*\{(.*?)\}\s*params\s*;', 'push')
    common = common.replace(text_push, '')
    text_ubo, ubo = block(r'layout\s*\([^)]*\)\s*uniform\s+UBO\s*\{(.*?)\}\s*global\s*;', 'ubo')
    common = common.replace(text_ubo, '')
    ubo = [(t, n) for t, n in ubo if n != 'MVP']
    members, push_size = layout_of(members)
    ubo, ubo_size = layout_of(ubo)
    push_text = struct_text('Push', members) + 'cbuffer Block : register(b0) { Push params; };\n'
    if ubo:
        push_text += struct_text('UBO', ubo) + 'cbuffer BlockU : register(b1) { UBO global; };\n'
    members = [('push',) + m for m in members] + [('ubo',) + m for m in ubo]

    results, textures = {}, []
    for stage, body in (('vertex', vert), ('fragment', frag)):
        body = body.split('\n', 1)[1]
        text = inline_self_macros(strip_comments(common + '\n' + body))
        text = text.replace('global.MVP * Position', 'Position').replace('global.MVP', '1')
        text, ins, outs, samplers = parse_layout_decls(text)
        names = [n for _, n in samplers]
        if stage == 'fragment':
            textures = [n for _, n in sorted(samplers)]
        text = translate_code(text, names)
        text = re.sub(r'\bvoid\s+main\s*\(\s*\)', 'void body_()', text)
        text, inits = static_globals(text)
        assert 'void body_()' in text
        text = re.sub(r'(void\s+body_\s*\(\s*\)\s*\{)', lambda m: m.group(1) + '\n' + '\n'.join(inits), text, count=1)
        head = [PRELUDE, push_text]
        for k, (bind, n) in enumerate(sorted(samplers)):
            head.append('Texture2D %s : register(t%d);\nSamplerState %s_s : register(s%d);' % (n, k, n, k))
            head.append('int2 %s_size() { uint w, h; %s.GetDimensions(w, h); return int2(w, h); }' % (n, n))
        if stage == 'vertex':
            head.append('static float4 gl_Position; static float4 Position; static float2 TexCoord;')
            for loc, typ, n in sorted(outs):
                head.append('static %s %s;' % (HLSL_TYPE[typ], n))
            head.append('struct VOut {\n  float4 pos_ : SV_Position;')
            for loc, typ, n in sorted(outs):
                head.append('  %s %s : TEXCOORD%d;' % (HLSL_TYPE[typ], n, loc))
            head.append('};')
            tail = ['VOut main(uint id : SV_VertexID) {',
                    '  float2 p = float2((id << 1) & 2, id & 2);',
                    '  TexCoord = p;',
                    '  Position = float4(p * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);',
                    '  body_();',
                    '  VOut o;',
                    '  o.pos_ = gl_Position;']
            tail += ['  o.%s = %s;' % (n, n) for _, _, n in sorted(outs)]
            tail += ['  return o;', '}']
        else:
            own_coord = '#define gl_FragCoord' not in text   # some shaders make their own from the texture coordinate
            if own_coord:
                head.append('static float4 gl_FragCoord;')
            for loc, typ, n in sorted(ins):
                head.append('static %s %s;' % (HLSL_TYPE[typ], n))
            for loc, typ, n in sorted(outs):
                head.append('static %s %s;' % (HLSL_TYPE[typ], n))
            head.append('struct PIn {\n  float4 pos_ : SV_Position;')
            for loc, typ, n in sorted(ins):
                head.append('  %s %s : TEXCOORD%d;' % (HLSL_TYPE[typ], n, loc))
            head.append('};')
            assert [n for _, _, n in outs] == ['FragColor'], (path, outs)
            tail = ['float4 main(PIn i) : SV_Target {'] + (['  gl_FragCoord = i.pos_;'] if own_coord else [])
            tail += ['  %s = i.%s;' % (n, n) for _, _, n in sorted(ins)]
            tail += ['  body_();', '  return FragColor;', '}']
        full = '\n'.join(head) + '\n' + text + '\n' + '\n'.join(tail) + '\n'
        for old, new, why in PATCHES.get(stem, []) if stage == 'fragment' else []:
            assert full.count(old) == 1, (stem, old, full.count(old))
            full = full.replace(old, new)
        full = re.sub(r'\n[ \t]*\n(?:[ \t]*\n)+', '\n\n', full)
        results[stage] = full
    return stem, params, members, textures, results


def parse_preset(path):
    kv = {}
    for line in read(path).split('\n'):
        line = line.split('#', 1)[0].strip()
        if '=' in line:
            k, v = line.split('=', 1)
            kv[k.strip()] = v.strip().strip('"')
    return kv


# Settings the 2600 look changes from the defaults of the shaders (name -> (value, reason)). None yet: the shader's own NTSC preset is used as it is.
OVERRIDES = {}

CHUNK = 7000   # a string literal of the compiler may not be longer than 16,380 bytes; the pieces are joined by the compiler


def cpp_string(text, tag):
    """HLSL text as adjacent raw string literals, cut at line ends."""
    pieces, cur = [], ''
    for line in text.split('\n'):
        if len(cur) + len(line) + 1 > CHUNK and cur:
            pieces.append(cur)
            cur = ''
        cur += line + '\n'
    pieces.append(cur)
    assert all(')HLSL"' not in p for p in pieces)
    return '\n    '.join('R"HLSL(%s)HLSL"' % p for p in pieces)


def scale_of(kv, key, i, axis):
    """(kind, factor) of a pass for 'x' or 'y'."""
    kind = kv.get('scale_type_%s%d' % (axis, i), kv.get('scale_type%d' % i, 'source'))
    factor = float(kv.get('scale_%s%d' % (axis, i), kv.get('scale%d' % i, '1.0')))
    assert kind in ('source', 'viewport', 'absolute'), kind
    return kind.capitalize(), factor


def flag(kv, key):
    return kv.get(key, 'false').lower() == 'true'


def emit(kv, passes, out_path):
    L = ['// Generated by tools/i11_slang_to_hlsl.py (Stage I11.4, DECISIONS.md T-I11-4): the crt-guest-advanced-ntsc preset of the libretro shader collection',
         '// (guest.r and Dr. Venom, GNU GPL version 2 or later; the NTSC passes are based on Themaister\'s NTSC shader), translated from Slang to HLSL.',
         '// The sources are in third_party/shaders (MANIFEST.txt). Do not edit by hand: change the tool and run it again.',
         '#include "gdv_data.hpp"', '', 'namespace gdv {', 'namespace {', '']
    all_params = {}
    for i, (stem, params, members, textures, hlsl) in enumerate(passes):
        L.append('const char kVertex%02d[] =\n    %s;' % (i, cpp_string(hlsl['vertex'], 'v')))
        L.append('const char kPixel%02d[] =\n    %s;' % (i, cpp_string(hlsl['fragment'], 'p')))
        L.append('const Member kMembers%02d[] = {' % i)
        for block, typ, name, off in members:
            kind = {'vec4': 'v', 'uint': 'u', 'float': 'f', 'int': 'i'}[typ]
            L.append('    {\'%s\', \'%s\', "%s", %d},' % (block[0], kind, name, off))
        L.append('};')
        L.append('const char* const kTextures%02d[] = {%s};' % (i, ', '.join('"%s"' % t for t in textures) if textures else 'nullptr'))
        L.append('')
        for name, value in params:
            assert all_params.setdefault(name, value) == value, ('the same setting with two defaults', name)
    L.append('}  // namespace\n')
    L.append('const Pass kPasses[] = {')
    for i, (stem, params, members, textures, hlsl) in enumerate(passes):
        sx, fx = scale_of(kv, 'scale', i, 'x')
        sy, fy = scale_of(kv, 'scale', i, 'y')
        L.append('    {"%s", kVertex%02d, kPixel%02d, kMembers%02d, %d, kTextures%02d, %d, %s, %s, %s, Scale::%s, Scale::%s, %.6ff, %.6ff, "%s"},' % (
            stem, i, i, i, len(members), i, len(textures), 'true' if flag(kv, 'filter_linear%d' % i) else 'false',
            'true' if flag(kv, 'float_framebuffer%d' % i) else 'false', 'true' if flag(kv, 'mipmap_input%d' % i) else 'false', sx, sy, fx, fy, kv.get('alias%d' % i, '')))
    L.append('};')
    L.append('const int kPassCount = %d;\n' % len(passes))
    L.append('const Param kParams[] = {')
    for name in sorted(all_params):
        value, why = OVERRIDES.get(name, (all_params[name], ''))
        text = '%.9g' % value
        L.append('    {"%s", %sf},' % (name, text if any(c in text for c in '.e') else text + '.0'))
    L.append('};')
    L.append('const int kParamCount = %d;\n' % len(all_params))
    L.append('}  // namespace gdv\n')
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(L))


def emit_luts(kv, out_path):
    names = kv['textures'].split(';')
    L = ['// Generated by tools/i11_slang_to_hlsl.py (Stage I11.4): the colour tables (PNG files, 1024 x 32) of the crt-guest-advanced preset, byte for byte from',
         '// third_party/shaders. Do not edit by hand.', '#include "gdv_data.hpp"', '', 'namespace gdv {', 'namespace {']
    for k, name in enumerate(names):
        data = open(os.path.join(CRT, kv[name]), 'rb').read()
        L.append('const unsigned char kLut%d[] = {' % k)
        for j in range(0, len(data), 24):
            L.append('    ' + ', '.join(str(b) for b in data[j:j + 24]) + ',')
        L.append('};')
    L.append('}  // namespace\n')
    L.append('const Lut kLuts[] = {')
    for k, name in enumerate(names):
        L.append('    {"%s", kLut%d, sizeof kLut%d, %s},' % (name, k, k, 'true' if flag(kv, name + '_linear') else 'false'))
    L.append('};')
    L.append('const int kLutCount = %d;\n' % len(names))
    L.append('}  // namespace gdv\n')
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(L))


def main():
    check_dir = None
    out_path = os.path.join(ROOT, 'src', 'host', 'gdv_generated.cpp')
    lut_path = os.path.join(ROOT, 'src', 'host', 'gdv_luts.cpp')
    args = sys.argv[1:]
    while args:
        a = args.pop(0)
        if a == '--check':
            check_dir = args.pop(0)
        elif a == '--out':
            out_path = args.pop(0)
        elif a == '--luts':
            lut_path = args.pop(0)
        else:
            raise SystemExit('unknown option ' + a)
    kv = parse_preset(PRESET)
    n = int(kv['shaders'])
    passes = []
    for i in range(n):
        path = os.path.join(CRT, kv['shader%d' % i])
        stem, params, members, textures, hlsl = translate_pass(path)
        print('pass %2d %-34s params %2d push %2d vertex %5d pixel %5d' % (i, stem, len(params), len(members), len(hlsl['vertex']), len(hlsl['fragment'])))
        passes.append((stem, params, members, textures, hlsl))
        if check_dir:
            os.makedirs(check_dir, exist_ok=True)
            for stage, ext in (('vertex', 'vs'), ('fragment', 'ps')):
                with open(os.path.join(check_dir, '%02d_%s.%s.hlsl' % (i, stem, ext)), 'w', encoding='utf-8', newline='\n') as f:
                    f.write(hlsl[stage])
    emit(kv, passes, out_path)
    emit_luts(kv, lut_path)


if __name__ == '__main__':
    main()
