#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <SFML/Audio.hpp>

#include "MMU.hpp"
#include "Cartridge.hpp"
#include "GPU.hpp"
#include "LR35902.hpp"
#include "GBAudioStream.hpp"

#include "CommandLine.hpp"

bool emulation_running = false;

// Options
bool debug_display = false;
bool use_bios = true;
bool with_sound = true;
bool debug = false;			// Pause execution
bool step = false;			// Step to next instruction (in debug)
bool frame_by_frame = true;
bool real_speed = true;
size_t sample_rate = 44100;	// Audio sample rate
size_t frame_skip = 0;		// Increase for better performances.

// Components of the emulated GameBoy
Cartridge	cartridge;
LR35902		cpu;
Gb_Apu		apu;
MMU			mmu;
GPU			gpu;

Stereo_Buffer gb_snd_buffer;
GBAudioStream snd_buffer;

// GUI
float screen_scale = 3.0;
sf::RenderWindow window;
sf::Texture	gameboy_screen;
sf::Texture	gameboy_logo_tex;
sf::Sprite gameboy_logo_sprite;

// Debug GUI
sf::Sprite gameboy_screen_sprite;
sf::Texture	gameboy_tilemap;
sf::Sprite gameboy_tilemap_sprite;
sf::Texture	gameboy_tilemap2;
sf::Sprite gameboy_tilemap_sprite2;
sf::Font font;
sf::Text debug_text;
sf::Text log_text;
sf::Text tilemap_text;
sf::Text tilemap_text2;
color_t* tile_map[2] = {nullptr, nullptr};

// Timing
sf::Clock timing_clock;
double frame_time = 0;
size_t speed_update = 10;
double speed = 100;
uint64_t elapsed_cycles = 0;
uint64_t speed_mesure_cycles = 0;
size_t frame_count = 0;

void handle_event(sf::Event event);
std::string get_debug_text();
void toggle_speed();
void reset();

void handle_event(sf::Event event)
{
    if (event.type == sf::Event::Closed)
        window.close();
    if (event.type == sf::Event::KeyPressed)
    {
        switch(event.key.code)
        {
            case sf::Keyboard::Escape: window.close(); break;
            case sf::Keyboard::Space: step = true; break;
            case sf::Keyboard::Return:
            {
                debug = !debug;
                elapsed_cycles = 0;
                timing_clock.restart();
                log_text.setString(debug ? "Debugging" : "Running");
                break;
            }
            case sf::Keyboard::BackSpace:
                reset();
                break;
            case sf::Keyboard::Add:
            {
                float vol = snd_buffer.getVolume();
                vol = std::min(vol + 10.0f, 100.0f);
                snd_buffer.setVolume(vol);
                log_text.setString(std::string("Volume at ").append(std::to_string(vol)));
                break;
            }
            case sf::Keyboard::Subtract:
            {
                float vol = snd_buffer.getVolume();
                vol = std::max(vol - 10.0f, 0.0f);
                snd_buffer.setVolume(vol);
                log_text.setString(std::string("Volume at ").append(std::to_string(vol)));
                break;
            }
            case sf::Keyboard::B:
            {
                addr_t addr;
                std::cout << "Adding breakpoint. Enter an address: " << std::endl;
                std::cin >> std::hex >> addr;
                cpu.add_breakpoint(addr);
                std::stringstream ss;
                ss << "Added a breakpoint at " << Hexa(addr) << ".";
                log_text.setString(ss.str());
                break;
            }
            case sf::Keyboard::N:
            {
                cpu.clear_breakpoints();
                log_text.setString("Cleared breakpoints.");
                break;
            }
            case sf::Keyboard::M: toggle_speed(); break;
            case sf::Keyboard::L:
            {
                frame_by_frame = true;
                step = true;
                debug = true;
                log_text.setString("Advancing one frame");
                break;
            }
            default: break;
        }
    } else if (event.type == sf::Event::JoystickButtonPressed) { // Joypad Interrupt
        switch(event.joystickButton.button)
        {
            case 0: mmu.key_down(MMU::Button, MMU::RightA); break;
            case 2:
            case 1: mmu.key_down(MMU::Button, MMU::LeftB); break;
            case 6: mmu.key_down(MMU::Button, MMU::UpSelect); break;
            case 7: mmu.key_down(MMU::Button, MMU::DownStart); break;
            case 5: toggle_speed(); break;
            default: break;
        }
    } else if (event.type == sf::Event::JoystickButtonReleased) {
        switch(event.joystickButton.button)
        {
            case 5: toggle_speed(); break;
            default: break;
        }
    } else if (event.type == sf::Event::JoystickMoved) { // Joypad Interrupt
        if (event.joystickMove.axis == sf::Joystick::X)
        {
            if(event.joystickMove.position > 50) mmu.key_down(MMU::Direction, MMU::RightA);
            else if(event.joystickMove.position < -50) mmu.key_down(MMU::Direction, MMU::LeftB);
        } else if (event.joystickMove.axis == sf::Joystick::Y) {
            if(event.joystickMove.position > 50) mmu.key_down(MMU::Direction, MMU::UpSelect);
            else if(event.joystickMove.position < -50) mmu.key_down(MMU::Direction, MMU::DownStart);
        }
    }
}

void toggle_speed()
{
    real_speed = !real_speed;
    elapsed_cycles = 0;
    timing_clock.restart();
    log_text.setString(real_speed ? "Running at real speed" : "Running as fast as possible");
}

void reset()
{
    gpu.reset();
    cpu.reset();
    if(use_bios)
    {
        mmu.load_bios();
    } else {
        cpu.reset_cart();
    }
    elapsed_cycles = 0;
    timing_clock.restart();
    debug_text.setString("Reset");
    size_t frame_cycles = (cpu.double_speed() ? cpu.frame_cycles / 2 : cpu.frame_cycles);
    apu.end_frame(frame_cycles);
}

MainCanvas::MainCanvas(QWidget* Parent, const QPoint& Position, const QSize& Size) :
    QSFMLCanvas(Parent, Position, Size)
{

}

void MainCanvas::OnInit()
{
    setVerticalSyncEnabled(false);

    if(!gameboy_screen.create(gpu.ScreenWidth, gpu.ScreenHeight))
        std::cerr << "Error creating the screen texture!" << std::endl;
    gameboy_screen_sprite.setTexture(gameboy_screen);
    gameboy_screen_sprite.setPosition(0, 0);
    gameboy_screen_sprite.setScale(screen_scale, screen_scale);

    mmu.cartridge = &cartridge;
    if(with_sound) cpu.apu = &apu;
    cpu.mmu = &mmu;
    gpu.mmu = &mmu;

    // Audio buffers
    gb_snd_buffer.clock_rate(LR35902::ClockRate);
    gb_snd_buffer.set_sample_rate(sample_rate);
    apu.output(gb_snd_buffer.center(), gb_snd_buffer.left(), gb_snd_buffer.right());
    snd_buffer.setVolume(50);

    // Input callbacks
    mmu.callback_joy_up = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) < -50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Up); };
    mmu.callback_joy_down = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::Y) > 50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Down); };
    mmu.callback_joy_right = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) > 50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Right); };
    mmu.callback_joy_left = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::getAxisPosition(i, sf::Joystick::X) < -50) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Left); };
    mmu.callback_joy_a = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 0)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::A); };
    mmu.callback_joy_b = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 1)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Z); };
    mmu.callback_joy_select = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 6)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::Q); };
    mmu.callback_joy_start = [&] () -> bool { for(int i = 0; i < sf::Joystick::Count; ++i) if(sf::Joystick::isConnected(i) && sf::Joystick::isButtonPressed(i, 7)) return true; return sf::Keyboard::isKeyPressed(sf::Keyboard::S); };
}

void MainCanvas::OnUpdate()
{
    if(!emulation_running)
    {
        return;
    }

    sf::Event event;
    while(pollEvent(event))
        handle_event(event);

    double gameboy_time = double(elapsed_cycles) / cpu.ClockRate;
    double diff = gameboy_time - timing_clock.getElapsedTime().asSeconds();
    if(real_speed && !debug && diff > 0)
        sf::sleep(sf::seconds(diff));
    if(!debug || step)
    {
        cpu.frame_cycles = 0;
        for(size_t i = 0; i < frame_skip + 1; ++i)
        {
            do
            {
                cpu.execute();
                if(cpu.get_instr_cycles() == 0)
                    break;

                size_t instr_cycles = (cpu.double_speed() ? cpu.get_instr_cycles() / 2 :
                                                            cpu.get_instr_cycles());
                gpu.step(instr_cycles, i == frame_skip);
                elapsed_cycles += instr_cycles;
                speed_mesure_cycles += instr_cycles;

                if(cpu.reached_breakpoint())
                {
                    std::stringstream ss;
                    ss << "Stepped on a breakpoint at " << Hexa(cpu.get_pc());
                    log_text.setString(ss.str());
                    debug = true;
                    step = false;
                    break;
                }
            } while((!debug || frame_by_frame) && !gpu.completed_frame());
            frame_count++;
        }

        if(with_sound)
        {
            size_t frame_cycles = (cpu.double_speed() ? cpu.frame_cycles / 2 : cpu.frame_cycles);
            bool stereo = apu.end_frame(frame_cycles);
            gb_snd_buffer.end_frame(frame_cycles, stereo);
            auto samples_count = gb_snd_buffer.samples_avail();
            if(samples_count > 0)
            {
                samples_count = gb_snd_buffer.read_samples(snd_buffer.add_samples(samples_count), samples_count);
                if(!(snd_buffer.getStatus() == sf::SoundSource::Status::Playing)) snd_buffer.play();
            }
        }

        gameboy_screen.update(reinterpret_cast<uint8_t*>(gpu.screen));

        frame_by_frame = false;
        step = false;
    }

    clear(sf::Color::Black);
    draw(gameboy_screen_sprite);

    emit state_update();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _debug_window(new DebugWindow)
{
    ui->setupUi(this);
    setWindowTitle("SenBoy");
    setAcceptDrops(true);
    resize(QSize(screen_scale * 160, screen_scale * 144 + menuBar()->height() + statusBar()->height()));
    setCentralWidget(new MainCanvas(this, QPoint(0, 0), QSize(screen_scale * 160, screen_scale * 144)));

    connect(centralWidget(), SIGNAL(state_update()), _debug_window, SLOT(update()));
}

MainWindow::~MainWindow()
{
    delete ui;
    delete _debug_window;
}

#include <QFileDialog>
#include <QMessageBox>

void MainWindow::open_rom(const QString& path)
{
    if(!cartridge.load(path.toStdString()))
    {
        QMessageBox::critical(this, tr("QMessageBox::critical()"),
                                            "Error opening ROM!",
                                            QMessageBox::Ok);
    } else {
        reset();
        emulation_running = true;
        statusBar()->showMessage(QString(cartridge.getName().c_str()));
    }
}

void MainWindow::action_open()
{
    auto path = QFileDialog::getOpenFileName(this,
        tr("Open ROM"), "", tr("Gameboy ROM (*.gb *.gbc)"));
    if(!path.isEmpty())
    {
        open_rom(path);
    }
}

#include <QDropEvent>
#include <QUrl>
#include <QDebug>
#include <QMimeData>


void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
   QList<QUrl> urls = event->mimeData()->urls();
   open_rom(urls.first().toLocalFile());
}

void MainWindow::on_actionDebug_Info_triggered()
{
    _debug_window->show();
}

DebugWindow::DebugWindow(QWidget* Parent) :
    QWidget(Parent),
    ui(new Ui::DebugWindow)
{
    ui->setupUi(this);

    connect(ui->Pause, SIGNAL(clicked(bool)), this, SLOT(pause()));
}

DebugWindow::~DebugWindow()
{
    delete ui;
}

void DebugWindow::update()
{
    ui->Disassembly->setText(QString::fromStdString(cpu.get_disassembly()));
    ui->PC_Val->setText(QString::fromStdString(Hexa(cpu.get_pc())));
    ui->SP_Val->setText(QString::fromStdString(Hexa(cpu.get_sp())));
    ui->AF_Val->setText(QString::fromStdString(Hexa(cpu.get_af())));
    ui->BC_Val->setText(QString::fromStdString(Hexa(cpu.get_bc())));
    ui->DE_Val->setText(QString::fromStdString(Hexa(cpu.get_de())));
    ui->HL_Val->setText(QString::fromStdString(Hexa(cpu.get_hl())));
}

void DebugWindow::pause()
{
    debug = !debug;
}
