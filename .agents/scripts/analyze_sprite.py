#!/usr/bin/env python3
"""
analyze_sprite.py — Pixel art sprite sheet analyzer with art crop detection.

Usage:
    python3 analyze_sprite.py <path/to/sprite.png> [frame_w] [frame_h]

Examples:
    python3 analyze_sprite.py assets/sprites/player/player.png
    python3 analyze_sprite.py assets/sprites/player/player.png 48 48
    python3 analyze_sprite.py assets/sprites/collectibles/coin.png 16 16

If frame_w / frame_h are omitted, the script attempts to auto-detect the frame
size by scanning for fully transparent column/row gutters in the sheet.

Outputs:
    - Sheet dimensions
    - Detected or provided frame size
    - Grid (cols x rows) and total frame count
    - Per-row frame transparency map (which frames are blank/used)
    - Per-frame art bounding box (the tight crop around visible pixels)
    - Suggested hitbox constants (ART_X, ART_Y, ART_W, ART_H or PAD style)
    - SDL_Rect C snippets for frame access and hitbox computation
"""

import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is not installed. Run: pip3 install Pillow")
    sys.exit(1)


# ── Transparent gutter detection ────────────────────────────────────────


def fully_transparent_col(pixels, height, x):
    """Return True if every pixel in column x has alpha == 0."""
    for y in range(height):
        if pixels[x, y][3] != 0:
            return False
    return True


def fully_transparent_row(pixels, width, y):
    """Return True if every pixel in row y has alpha == 0."""
    for x in range(width):
        if pixels[x, y][3] != 0:
            return False
    return True


# ── Frame-size auto-detection ───────────────────────────────────────────


def find_gutter_period(is_transparent, total):
    """
    Given a boolean list where True = fully transparent line, find the
    smallest repeating period P such that every P-th line is transparent
    and the interval contains at least some non-transparent lines.

    This detects the frame grid by looking for transparent separators.
    """
    for p in range(8, total + 1):
        if total % p != 0:
            continue
        # Check if lines at every p boundary are transparent
        all_match = True
        has_content = False
        for i in range(total):
            if i % p == 0:
                # Boundary lines don't need to be transparent for edgeless sheets
                pass
            else:
                if not is_transparent[i]:
                    has_content = True
        if has_content:
            # Verify: does this period produce a sensible grid? (1-32 divisions)
            divisions = total // p
            if 1 <= divisions <= 32:
                return p
    return None


def _frame_has_pixels(pixels, x0, y0, fw, fh):
    """Return True if any pixel in the frame cell has alpha > 0."""
    for dy in range(fh):
        for dx in range(fw):
            if pixels[x0 + dx, y0 + dy][3] > 0:
                return True
    return False


def _count_used_cells(pixels, w, h, fw, fh):
    """Count how many grid cells contain at least one opaque pixel."""
    cols, rows = w // fw, h // fh
    used = 0
    for r in range(rows):
        for c in range(cols):
            if _frame_has_pixels(pixels, c * fw, r * fh, fw, fh):
                used += 1
    return used


def detect_frame_size(img_rgba):
    """
    Detect frame size using multiple strategies:
    1. Transparent gutter scanning (most reliable for padded sheets)
    2. Content-aware scoring of common pixel-art sizes
    3. Fallback: whole sheet as one frame

    The content-aware scorer checks each candidate grid and prefers sizes
    where most cells contain art (high fill ratio), art is inset within
    cells (has transparent padding), and frames are square-ish.

    Returns (frame_w, frame_h, cols, rows).
    """
    w, h = img_rgba.size
    pixels = img_rgba.load()

    # Strategy 1: Find transparent column/row gutters
    col_transparent = [fully_transparent_col(pixels, h, x) for x in range(w)]
    row_transparent = [fully_transparent_row(pixels, w, y) for y in range(h)]

    fw = _detect_cluster_size(col_transparent, w)
    fh = _detect_cluster_size(row_transparent, h)

    if fw and fh:
        cols, rows = w // fw, h // fh
        if 1 <= cols <= 32 and 1 <= rows <= 32:
            return fw, fh, cols, rows

    # Strategy 2: Score common pixel-art frame sizes using actual content
    # For small sheets (both dimensions <= 64), if no gutters were found,
    # the whole sheet is almost certainly a single tile/sprite — skip
    # subdivision attempts that would just carve up a circular saw or coin.
    if w <= 64 and h <= 64:
        return w, h, 1, 1

    # Common frame sizes in pixel art games — these are strongly preferred
    # when they divide evenly. Ordered by likelihood in this project.
    preferred = [48, 64, 32, 96, 128, 16, 24, 8]
    candidates = preferred

    best = None
    best_score = -1.0

    for fw_c in candidates:
        for fh_c in candidates:
            if w % fw_c != 0 or h % fh_c != 0:
                continue
            cols = w // fw_c
            rows = h // fh_c
            if not (1 <= cols <= 32 and 1 <= rows <= 32):
                continue

            total = cols * rows
            used = _count_used_cells(pixels, w, h, fw_c, fh_c)
            if used == 0:
                continue

            # Fill ratio: what fraction of cells have content?
            fill = used / total

            # Aspect ratio: prefer square or near-square frames
            aspect = min(fw_c, fh_c) / max(fw_c, fh_c)

            # Frame count: prefer 2-8 used frames (typical animation range)
            if used == 1:
                count_bonus = 0.3
            elif 2 <= used <= 8:
                count_bonus = 1.0
            elif used <= 16:
                count_bonus = 0.6
            else:
                count_bonus = 0.2

            # Padding bonus: check if used frames have transparent padding.
            # Real sprite frames have art inset within the frame bounds.
            # Sample up to 4 used frames.
            pad_bonus = 0.0
            sampled = 0
            for r in range(rows):
                for c in range(cols):
                    if sampled >= 4:
                        break
                    x0, y0 = c * fw_c, r * fh_c
                    if _frame_has_pixels(pixels, x0, y0, fw_c, fh_c):
                        edges_clear = 0
                        if all(pixels[x0 + dx, y0][3] == 0 for dx in range(fw_c)):
                            edges_clear += 1
                        if all(pixels[x0 + dx, y0 + fh_c - 1][3] == 0 for dx in range(fw_c)):
                            edges_clear += 1
                        if all(pixels[x0, y0 + dy][3] == 0 for dy in range(fh_c)):
                            edges_clear += 1
                        if all(pixels[x0 + fw_c - 1, y0 + dy][3] == 0 for dy in range(fh_c)):
                            edges_clear += 1
                        pad_bonus += edges_clear / 4.0
                        sampled += 1
                if sampled >= 4:
                    break
            if sampled > 0:
                pad_bonus /= sampled

            # Penalize when no frames have any padding — subdividing a tile
            if pad_bonus == 0.0 and total > 1:
                pad_bonus = -2.0

            # Preference bonus: strongly prefer frame sizes common in this
            # game (48, 64) over unusual ones (24, 8). This prevents the
            # scorer from picking 24×24 when 48×48 fits the actual grid.
            pref_bonus = 0.0
            if fw_c in (48, 64) and fh_c in (48, 64):
                pref_bonus = 3.0
            elif fw_c in (32, 96) and fh_c in (32, 96):
                pref_bonus = 1.5
            elif fw_c == 16 and fh_c == 16:
                pref_bonus = 1.0

            # Combined score
            score = (fill * 2.0          # reward high fill ratio
                     + aspect * 1.0      # mild preference for square frames
                     + count_bonus * 2.0 # reward typical animation frame counts
                     + pad_bonus * 2.0   # reward frames with transparent padding
                     + pref_bonus)       # prefer game's common frame sizes

            if score > best_score:
                best_score = score
                best = (fw_c, fh_c, cols, rows)

    if best:
        return best

    # Last resort: whole sheet is one frame
    return w, h, 1, 1


def _detect_cluster_size(transparent_map, total):
    """
    Find content clusters in a transparency map. A cluster is a run of
    non-transparent lines. Returns the cluster period if consistent, else None.
    """
    # Find edges: transitions from transparent to content
    edges = []
    in_content = False
    for i in range(total):
        if not transparent_map[i] and not in_content:
            edges.append(i)
            in_content = True
        elif transparent_map[i] and in_content:
            in_content = False

    if len(edges) < 2:
        return None

    # Check if edges are evenly spaced
    gaps = [edges[i + 1] - edges[i] for i in range(len(edges) - 1)]
    if not gaps:
        return None

    period = gaps[0]
    if all(g == period for g in gaps) and total % period == 0:
        return period

    return None


# ── Art bounding box (tight crop) ──────────────────────────────────────


def get_art_bbox(pixels, x0, y0, fw, fh):
    """
    Find the tightest bounding box around non-transparent pixels within
    a single frame cell starting at (x0, y0) with size fw x fh.

    Returns (art_x, art_y, art_w, art_h) relative to the frame's top-left,
    or None if the frame is fully transparent.
    """
    min_x, min_y = fw, fh
    max_x, max_y = -1, -1

    for dy in range(fh):
        for dx in range(fw):
            if pixels[x0 + dx, y0 + dy][3] > 0:
                if dx < min_x:
                    min_x = dx
                if dx > max_x:
                    max_x = dx
                if dy < min_y:
                    min_y = dy
                if dy > max_y:
                    max_y = dy

    if max_x < 0:
        return None  # fully transparent frame

    return (min_x, min_y, max_x - min_x + 1, max_y - min_y + 1)


def get_frame_colors(pixels, x0, y0, fw, fh):
    """
    Collect all unique non-transparent colors in a frame cell.
    Returns a set of (r, g, b, a) tuples.
    """
    colors = set()
    for dy in range(fh):
        for dx in range(fw):
            px = pixels[x0 + dx, y0 + dy]
            if px[3] > 0:
                colors.add(px)
    return colors


# ── Per-row analysis ────────────────────────────────────────────────────


def count_used_frames_in_row(pixels, row, cols, fw, fh, img_h):
    """Return a list of booleans: True = frame has at least one opaque pixel."""
    used = []
    y_start = row * fh
    y_end = min(y_start + fh, img_h)
    for col in range(cols):
        x_start = col * fw
        x_end = x_start + fw
        has_pixel = False
        for y in range(y_start, y_end):
            for x in range(x_start, x_end):
                if pixels[x, y][3] > 0:
                    has_pixel = True
                    break
            if has_pixel:
                break
        used.append(has_pixel)
    return used


# ── Hitbox style detection ──────────────────────────────────────────────


def suggest_hitbox_style(fw, fh, art_x, art_y, art_w, art_h):
    """
    Decide which constant style to recommend based on the art's position
    within the frame. The game uses two patterns:

    1. ART offset style (used when art is off-center or top-heavy):
       ART_X, ART_Y, ART_W, ART_H — direct offset from frame origin

    2. PAD style (used when art is roughly centered):
       HITBOX_PAD_X, HITBOX_PAD_Y — symmetric inset from frame edges

    Returns ('ART', {}) or ('PAD', {}) with the constant values.
    """
    pad_left = art_x
    pad_right = fw - (art_x + art_w)
    pad_top = art_y
    pad_bottom = fh - (art_y + art_h)

    # If left and right padding are equal (±1 px), suggest PAD style
    if abs(pad_left - pad_right) <= 1:
        pad_x = (pad_left + pad_right) // 2
        return ('PAD', {
            'pad_x': pad_x,
            'pad_y': pad_top,
            'result_w': fw - 2 * pad_x,
            'result_h': fh - pad_top - pad_bottom,
            'pad_bottom': pad_bottom,
        })

    # Otherwise use ART offset style
    return ('ART', {
        'art_x': art_x,
        'art_y': art_y,
        'art_w': art_w,
        'art_h': art_h,
    })


# ── Main analysis ──────────────────────────────────────────────────────


def analyze(path, frame_w=None, frame_h=None):
    img = Image.open(path).convert("RGBA")
    w, h = img.size
    pixels = img.load()

    print(f"\n{'=' * 60}")
    print(f"  Sprite Sheet: {Path(path).name}")
    print(f"{'=' * 60}")
    print(f"  Sheet size : {w} x {h} px")

    if frame_w and frame_h:
        fw, fh = frame_w, frame_h
        if w % fw != 0 or h % fh != 0:
            print(f"  WARNING: {w}x{h} is not evenly divisible by {fw}x{fh}")
        cols = max(w // fw, 1)
        rows = max(h // fh, 1)
        print(f"  Frame size : {fw} x {fh} px  (provided)")
    else:
        fw, fh, cols, rows = detect_frame_size(img)
        print(f"  Frame size : {fw} x {fh} px  (auto-detected)")

    total = cols * rows
    print(f"  Grid       : {cols} cols x {rows} rows  =  {total} frames")
    print()

    # ── Per-row frame map ───────────────────────────────────────────
    print("  Per-row frame map  (# = has pixels, . = transparent/empty)")
    print(f"  {'Row':<5}  {'Frames':}")
    for row in range(rows):
        used = count_used_frames_in_row(pixels, row, cols, fw, fh, h)
        symbols = "  ".join("#" if u else "." for u in used)
        used_count = sum(used)
        print(f"  {row:<5}  {symbols}   ({used_count} used)")

    print()

    # ── Per-frame art bounding box (crop analysis) ──────────────────
    print("-" * 60)
    print("  Art Bounding Box Analysis (tight crop per frame)")
    print("-" * 60)

    all_bboxes = []
    all_colors = set()

    for row in range(rows):
        for col in range(cols):
            x0 = col * fw
            y0 = row * fh
            bbox = get_art_bbox(pixels, x0, y0, fw, fh)
            colors = get_frame_colors(pixels, x0, y0, fw, fh)
            all_colors |= colors

            if bbox:
                ax, ay, aw, ah = bbox
                all_bboxes.append((row, col, ax, ay, aw, ah))
                print(f"  [{row},{col}]  art at ({ax},{ay})  size {aw}x{ah}"
                      f"  padding: L={ax} T={ay} R={fw - ax - aw} B={fh - ay - ah}"
                      f"  colors={len(colors)}")

    if not all_bboxes:
        print("  (no visible pixels found)")
        print(f"{'=' * 60}\n")
        return

    # ── Envelope: union of all frame bounding boxes ─────────────────
    # This is the overall art region across ALL frames — the tightest
    # rectangle that contains every frame's art without clipping any.
    env_x = min(b[2] for b in all_bboxes)
    env_y = min(b[3] for b in all_bboxes)
    env_x2 = max(b[2] + b[4] for b in all_bboxes)
    env_y2 = max(b[3] + b[5] for b in all_bboxes)
    env_w = env_x2 - env_x
    env_h = env_y2 - env_y

    print()
    print("-" * 60)
    print("  Envelope (union of all frames' art bounds)")
    print("-" * 60)
    print(f"  Origin : ({env_x}, {env_y})  within {fw}x{fh} frame")
    print(f"  Size   : {env_w} x {env_h} px")
    print(f"  Padding: L={env_x}  T={env_y}  R={fw - env_x2}  B={fh - env_y2}")
    print(f"  Coverage: {env_w * env_h}/{fw * fh} px "
          f"({100 * env_w * env_h / (fw * fh):.1f}% of frame)")
    print()

    # ── Color palette summary ───────────────────────────────────────
    opaque_colors = {(r, g, b) for r, g, b, a in all_colors if a == 255}
    semi_colors = {c for c in all_colors if c[3] > 0 and c[3] < 255}
    print("-" * 60)
    print("  Color Palette")
    print("-" * 60)
    print(f"  Unique opaque colors : {len(opaque_colors)}")
    print(f"  Semi-transparent     : {len(semi_colors)}")
    print(f"  Total unique (a > 0) : {len(all_colors)}")
    if opaque_colors and len(opaque_colors) <= 24:
        sorted_colors = sorted(opaque_colors, key=lambda c: (c[0], c[1], c[2]))
        for i, (r, g, b) in enumerate(sorted_colors):
            hex_c = f"#{r:02X}{g:02X}{b:02X}"
            print(f"    {i:>2}. {hex_c}  rgb({r},{g},{b})")
    elif opaque_colors:
        print(f"    (too many to list — showing first 16)")
        sorted_colors = sorted(opaque_colors, key=lambda c: (c[0], c[1], c[2]))
        for i, (r, g, b) in enumerate(sorted_colors[:16]):
            hex_c = f"#{r:02X}{g:02X}{b:02X}"
            print(f"    {i:>2}. {hex_c}  rgb({r},{g},{b})")
    print()

    # ── Hitbox constant suggestions ─────────────────────────────────
    style, vals = suggest_hitbox_style(fw, fh, env_x, env_y, env_w, env_h)

    print("-" * 60)
    print("  Suggested Hitbox Constants")
    print("-" * 60)
    print()

    # Derive a prefix from the filename
    stem = Path(path).stem.upper()
    # Common prefix cleanup
    prefix = stem.replace("-", "_")

    if style == 'ART':
        ax, ay, aw, ah = vals['art_x'], vals['art_y'], vals['art_w'], vals['art_h']
        print(f"  /* Art occupies cols {ax}..{ax + aw - 1}, rows {ay}..{ay + ah - 1}"
              f" within the {fw}x{fh} frame */")
        print(f"  #define {prefix}_FRAME_W  {fw}")
        print(f"  #define {prefix}_FRAME_H  {fh}")
        print(f"  #define {prefix}_ART_X    {ax}"
              f"    /* first visible column */")
        print(f"  #define {prefix}_ART_Y    {ay}"
              f"    /* first visible row    */")
        print(f"  #define {prefix}_ART_W    {aw}"
              f"    /* width of visible art  */")
        print(f"  #define {prefix}_ART_H    {ah}"
              f"    /* height of visible art */")
        print()
        print("  /* Hitbox using ART offset style (matches debug.c convention): */")
        print(f"  SDL_Rect hitbox_get(const Entity *e) {{")
        print(f"      return (SDL_Rect){{")
        print(f"          .x = (int)e->x + {prefix}_ART_X,")
        print(f"          .y = (int)e->y + {prefix}_ART_Y,")
        print(f"          .w = {prefix}_ART_W,")
        print(f"          .h = {prefix}_ART_H,")
        print(f"      }};")
        print(f"  }}")
    else:
        px = vals['pad_x']
        py = vals['pad_y']
        pb = vals['pad_bottom']
        rw = vals['result_w']
        rh = vals['result_h']
        print(f"  /* Art is centered horizontally (pad {px}px each side),"
              f" top-pad {py}px, bottom-pad {pb}px */")
        print(f"  #define {prefix}_FRAME_W       {fw}")
        print(f"  #define {prefix}_FRAME_H       {fh}")
        print(f"  #define {prefix}_HITBOX_PAD_X   {px}"
              f"    /* left/right inset    */")
        print(f"  #define {prefix}_HITBOX_PAD_Y   {py}"
              f"    /* top inset           */")
        print()
        print(f"  /* Resulting hitbox: {rw} x {rh} px */")
        print(f"  SDL_Rect hitbox_get(const Entity *e) {{")
        print(f"      return (SDL_Rect){{")
        print(f"          .x = (int)e->x + {prefix}_HITBOX_PAD_X,")
        print(f"          .y = (int)e->y + {prefix}_HITBOX_PAD_Y,")
        print(f"          .w = {prefix}_FRAME_W - 2 * {prefix}_HITBOX_PAD_X,")
        print(f"          .h = {prefix}_FRAME_H - {prefix}_HITBOX_PAD_Y - {pb},")
        print(f"      }};")
        print(f"  }}")

    print()

    # ── SDL_Rect source rect snippet ────────────────────────────────
    print("-" * 60)
    print("  SDL_Rect C Snippets")
    print("-" * 60)
    print()
    print("  /* Source rect for frame (col, row): */")
    print(f"  #define FRAME_W  {fw}")
    print(f"  #define FRAME_H  {fh}")
    print(f"  SDL_Rect src = {{")
    print(f"      .x = col * FRAME_W,")
    print(f"      .y = row * FRAME_H,")
    print(f"      .w = FRAME_W,")
    print(f"      .h = FRAME_H,")
    print(f"  }};")
    print()

    # ── Animation row summary ───────────────────────────────────────
    if rows > 1:
        print("-" * 60)
        print("  Animation Rows")
        print("-" * 60)
        for row in range(rows):
            row_frames = [b for b in all_bboxes if b[0] == row]
            if row_frames:
                # Check if frames vary (animation) or are identical (static)
                widths = set(b[4] for b in row_frames)
                heights = set(b[5] for b in row_frames)
                varying = len(widths) > 1 or len(heights) > 1
                note = " (varying sizes)" if varying else ""
                print(f"  Row {row}: {len(row_frames)} frames{note}")
            else:
                print(f"  Row {row}: (empty)")
        print()

    print(f"{'=' * 60}\n")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(0)

    sprite_path = sys.argv[1]
    fw = int(sys.argv[2]) if len(sys.argv) >= 3 else None
    fh = int(sys.argv[3]) if len(sys.argv) >= 4 else fw  # square default

    analyze(sprite_path, fw, fh)
