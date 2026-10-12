// Explicitly exercise the real VGUI/CoreText font registration, including
// Valve's encrypted font fallback. This runs only when the command is invoked.
#include "cbase.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"

CON_COMMAND_F(m4_font_check, "Register original TTF/VFONT through the real VGUI interface", FCVAR_RELEASE)
{
    const bool ttf = vgui::surface()->AddCustomFontFile("vgui/fonts/marlett.ttf");
    const bool vfont = vgui::surface()->AddCustomFontFile("vgui/fonts/univercl.vfont");
    vgui::HFont font = vgui::surface()->CreateFont();
    const bool glyphSet = vgui::surface()->SetFontGlyphSet(font, "Marlett", 16, 400, 0, 0, 0);
    int a = 0, b = 0, c = 0;
    if (glyphSet) vgui::surface()->GetCharABCwide(font, '4', a, b, c);
    Msg("M4_FONT ttf=%d vfont=%d glyph_set=%d abc=%d,%d,%d\n", ttf, vfont, glyphSet, a, b, c);
}
