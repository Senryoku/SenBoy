#pragma once

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include "wxSFMLCanvas.hpp"
#include "MMU.hpp"
#include "Cartridge.hpp"
#include "GPU.hpp"
#include "LR35902.hpp"
#include "GBAudioStream.hpp"

class MainCanvas : public wxSFMLCanvas
{
public:
    MainCanvas(wxWindow*  	Parent,
				 wxWindowID	Id,
				 wxPoint	Position,
				 wxSize 	Size,
				 long       Style = 0);

	sf::Texture		gameboy_screen;
	sf::Sprite		gameboy_screen_sprite;
	
	void init();
private:
    sf::Event		_event;
    wxFrame*		_parent;
	
    virtual void OnUpdate() override;
	void handle_event(sf::Event event);
};

class MainFrame : public wxFrame
{
public:
    MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	
private:
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnOpen(wxCommandEvent&);
	
    wxDECLARE_EVENT_TABLE();
};

class SenBoy : public wxApp
{
public:
    virtual bool OnInit();
};

