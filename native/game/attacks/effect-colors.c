/* Shared integer colour conversion for native RGBA32 effect artwork. */
unsigned int lc_effect_color(unsigned int rgba, int mode, int degrees, unsigned int color) {
    int r = rgba >> 24, g = (rgba >> 16) & 255, b = (rgba >> 8) & 255;
    int hi = r > g ? r : g, lo = r < g ? r : g, d, h, sector, f, p, q, t;
    if (b > hi) hi = b;
    if (b < lo) lo = b;
    if (mode == 2) {
        r = hi * ((color >> 16) & 255) / 255;
        g = hi * ((color >> 8) & 255) / 255;
        b = hi * (color & 255) / 255;
    } else if (mode == 1 && (d = hi - lo) != 0) {
        h = hi == r ? (g - b) * 256 / d : hi == g ? 512 + (b - r) * 256 / d
                                                          : 1024 + (r - g) * 256 / d;
        h = (h + degrees * 256 / 60 + 3072) % 1536;
        sector = h / 256; f = h % 256;
        p = lo; q = hi - d * f / 256; t = lo + d * f / 256;
        if (sector == 0) { r = hi; g = t; b = p; }
        else if (sector == 1) { r = q; g = hi; b = p; }
        else if (sector == 2) { r = p; g = hi; b = t; }
        else if (sector == 3) { r = p; g = q; b = hi; }
        else if (sector == 4) { r = t; g = p; b = hi; }
        else { r = hi; g = p; b = q; }
    }
    return ((unsigned int)r << 24) | ((unsigned int)g << 16) | ((unsigned int)b << 8) | (rgba & 255);
}
