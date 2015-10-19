#include "wxSFMLCanvas.hpp"

#ifdef __WXGTK__
    #include <gdk/gdkx.h>
    #include <gtk/gtk.h>
    #include <wx/gtk/win_gtk.h>
#endif

wxSFMLCanvas::wxSFMLCanvas(wxWindow* Parent, wxWindowID Id, const wxPoint& Position, const wxSize& Size, long Style) :
wxControl(Parent, Id, Position, Size, Style)
{
    #ifdef __WXGTK__

        // La version GTK requiert d'aller plus profondément pour trouver
        // l'identificateur X11 du contrôle
        gtk_widget_realize(m_wxwindow);
        gtk_widget_set_double_buffered(m_wxwindow, false);
        GdkWindow* Win = GTK_PIZZA(m_wxwindow)->bin_window;
        XFlush(GDK_WINDOW_XDISPLAY(Win));
        sf::RenderWindow::create(GDK_WINDOW_XWINDOW(Win));

    #else

        // Testé sous Windows XP seulement (devrait fonctionner sous X11 et
        // les autres versions de Windows - aucune idée concernant MacOS)
        sf::RenderWindow::create(GetHandle());

    #endif
}

void wxSFMLCanvas::OnIdle(wxIdleEvent&)
{
	Refresh();
}

void wxSFMLCanvas::OnPaint(wxPaintEvent&)
{
	wxPaintDC Dc(this);
	OnUpdate();
	display();
}