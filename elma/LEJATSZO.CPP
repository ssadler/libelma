#include "LEJATSZO.H"
#include "EDITUJ.H"
#include "eol/console.h"
#include "eol_settings.h"
#include "keys.h"
#include "flagtag.h"
#include "fs_utils.h"
#include "sound_engine.h"
#include "KIRAJZOL.H"
#include "physics_forces.h"
#include "level.h"
#include "lgr.h"
#include "main.h"
#include "object.h"
#include "physics_init.h"
#include "platform_impl.h"
#include "segments.h"
#include "timer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <directinput/scancodes.h>
#include <filesystem>

// Jatek modja ket fele lehet:
int Single = 1;
// Van-e fogocska:
int Tag = 0;
// int Multi = 0;

int Legkozment = 0;
bool VideoRecordingMode = false;
int VideoFrameIndex = 0;
std::string VideoOutputDirectory;

static int Kajakell;

bool handle_console_input() {
    bool was_active = Console->is_input_active();
    if (was_key_just_pressed(DIK_F9)) {
        Console->toggle_active();
    } else if (Console->is_input_active()) {
        Console->handle_input();
    }
    return was_active;
}

static bool is_game_key_down(DikScancode code) {
    if (Console->is_input_active()) {
        return false;
    }
    return is_key_down(code);
}

bool was_game_key_just_pressed(DikScancode code) {
    if (Console->is_input_active()) {
        return false;
    }
    return was_key_just_pressed(code);
}

void update_freecam(double dt, camera& current_camera) {
    double speed = 30.0;
    if (is_game_key_down(DIK_LSHIFT) || is_game_key_down(DIK_RSHIFT)) {
        speed *= 4.0;
    }
    double move = speed * dt;
    if (is_game_key_down(DIK_UP)) {
        current_camera.y += move;
    }
    if (is_game_key_down(DIK_DOWN)) {
        current_camera.y -= move;
    }
    if (is_game_key_down(DIK_LEFT)) {
        current_camera.x -= move;
    }
    if (is_game_key_down(DIK_RIGHT)) {
        current_camera.x += move;
    }

    current_camera.x = std::clamp(current_camera.x, current_camera.min_x, current_camera.max_x);
    current_camera.y = std::clamp(current_camera.y, current_camera.min_y, current_camera.max_y);
}

//struct viewtimest {
//    int viewkinplay, viewkinreplay;
//    int timekinplay, timekinreplay;
//};

void initsoundlejatszoban() {
    static int Hangmarmegy = 0;
    if (State->sound_on && !Hangmarmegy) {
        Hangmarmegy = 1;
        sound_engine_init();
    }
}

//// Player 1 es 2-re:
static viewtimest Viewtime1 = {1, 0, 1, 0};
static viewtimest Viewtime2 = {1, 0, 1, 0};

BikeState spritefeldolgoz(int sorszam, int* pkajaszam, motorst* pmot) {
    if (sorszam < 0 || sorszam >= MAX_OBJECTS) {
    printf("bad obj id: %i\n", sorszam);
        internal_error("spritefeldolgoz-ban sorszam < 0 || sorszam >= MAXKEREK!");
    }
    if (!Ptop->objects[sorszam]) {
        internal_error("spritefeldolgoz-ban !Ptop->objects[sorszam]!");
    }

    object::Type tipus = Ptop->objects[sorszam]->type;

    if (tipus == object::Type::Killer) {
        return BikeState::Dead;
    }
    if (tipus == object::Type::Food) {
        Ptop->objects[sorszam]->active = false;
        (*pkajaszam)++;
        add_event_buffer(WavEvent::Food, 0.99, -1);
        switch (Ptop->objects[sorszam]->property) {
        case object::Property::None:
            break;
        case object::Property::GravityUp:
            pmot->gravity_direction = MotorGravity::Up;
            break;
        case object::Property::GravityDown:
            pmot->gravity_direction = MotorGravity::Down;
            break;
        case object::Property::GravityLeft:
            pmot->gravity_direction = MotorGravity::Left;
            break;
        case object::Property::GravityRight:
            pmot->gravity_direction = MotorGravity::Right;
            break;
        }
        return BikeState::Normal;
    }
    if (tipus == object::Type::Exit) {
        return BikeState::Finish;
    }
    return BikeState::Normal;
}

static double baljobbszamol(double eltelt, int hatra) {
    eltelt = std::max<double>(eltelt, 0);

    double camera_flip_time = EolSettings->turn_time() + 0.15;
    if (hatra) {
        if (eltelt > camera_flip_time) {
            return 0.0;
        }
        return 1.0 - eltelt / camera_flip_time;
    } else {
        if (eltelt > camera_flip_time) {
            return 1.0;
        }
        return eltelt / camera_flip_time;
    }
}

static void toggleresz(player_keys* popciok, valtozok* pvalt, int* pviewkin, int* ptimekin,
                       int* pmasikshow) {
    // Showkep:
    if (was_game_key_just_pressed(popciok->toggle_visibility)) {
        Kitoltestmegrak = Kitoltestmegrakkezd;
        if (!*pmasikshow) {
            // Ha masik jatekos ablaka nem latszott, mindegyik fog latszani:
            *pmasikshow = 1;
            pvalt->showkep = 1;
        } else {
            pvalt->showkep = !pvalt->showkep;
        }
    }
    // View ablak:
    if (was_game_key_just_pressed(popciok->toggle_minimap)) {
        *pviewkin = !*pviewkin;
    }

    // Ido kint legyen vagy ne:
    if (was_game_key_just_pressed(popciok->toggle_timer)) {
        *ptimekin = !*ptimekin;
    }
}

static void baljobbelintez(baljobbvaltozok* pvalt, recorder* prec, double eddig, int hatra) {
    if (pvalt->eddighatra != hatra) {
        pvalt->ucsoforgas = eddig;
        double eltelt = eddig - pvalt->ucsoford;
        double camera_flip_time = EolSettings->turn_time() + 0.15;
        if (camera_flip_time > 0.0 && eltelt < camera_flip_time) {
            pvalt->ucsoford = eddig + eltelt - camera_flip_time;
        } else {
            pvalt->ucsoford = eddig;
        }
        pvalt->eddighatra = hatra;
        // Hang adas:
        //if (prec) {
        //    // Most kihagyjuk wavgyujtot:
        //    start_wav(WavEvent::Turn, 0.99);
        //    prec->store_event(eddig, WavEvent::Turn, 0.99, -1);
        //}
    }

    // Kirajzolashoz kellenek:
    double turn_time = EolSettings->turn_time();
    if (turn_time == 0.0) {
        // Instant turn
        pvalt->forgas = 1.0;
    } else {
        pvalt->forgas = (eddig - pvalt->ucsoforgas) / turn_time;

        pvalt->forgas = std::clamp(pvalt->forgas, 0.0, 1.0);
    }

    pvalt->baljobb = baljobbszamol(eddig - pvalt->ucsoford, hatra);
}

void kulsoresz(motorst* pmot, player_keys* popciok, valtozok* pvalt, recorder* prec,
                      viewtimest* pvt, double eddig, int* pmasikshow, int meghalt, int keys) {
    toggleresz(popciok, pvalt, &pvt->viewkinplay, &pvt->timekinplay, pmasikshow);

    // Hatra fordulas elintezes:
    if (!meghalt) {
        bool is_turn_down = (keys & 16) > 0;

        //if (EolSettings->cripple_no_turn()) {
        //    is_turn_down = false;
        //}
        //if (EolSettings->cripple_one_turn() && pvalt->one_turn_used) {
        //    is_turn_down = false;
        //}
        if (!pvalt->hatranyomva && is_turn_down) {
            pmot->flipped_bike = !pmot->flipped_bike;
            set_head_position(pmot);
            //pvalt->one_turn_used = true;
        }
        pvalt->hatranyomva = is_turn_down;
    }
    baljobbelintez(&pvalt->baljobbv_f, prec, eddig, pmot->flipped_bike);

    // Hatra elintezes:
    if (pmot->gravity_direction == MotorGravity::Up) {
        pmot->flipped_camera = !pmot->flipped_bike;
    } else {
        pmot->flipped_camera = pmot->flipped_bike;
    }
    baljobbelintez(&pvalt->baljobbv_h, nullptr, eddig, pmot->flipped_camera);

    // Kirajzolashoz kell:
    pvalt->ugrasnagysag = 1.0 - (eddig - pvalt->utolsougras) / VoltDelay;
    if (pvalt->ugrasnagysag < 0) {
        pvalt->ugrasnagysag = 0;
    }
#ifdef DEBUG
    if (pvalt->ugrasnagysag > 1.00001) {
        internal_error("pvalt->ugrasnagysag > 1.00001!");
    }
#endif
}

// Common setup function
void setup_gameloop(const char* filenev, motorst* motor) {
    load_best_time(filenev, Single);

    init_physics_data();

    if (!Ptop) {
        internal_error("setup_gameloop() !Ptop!");
    }
    Ptop->flip_objects();
    Ptop->sort_objects();
    // setallaktiv allitja be motor kezdeti helyzetet is!:
    Kajakell = Ptop->initialize_objects(motor);
    //Ptop->initialize_objects(Motor2);

    // Eloszor keretet tobbszor is kirakja kirajzol320:
    Kitoltestmegrak = Kitoltestmegrakkezd;

    flagtag_reset();

    motor->apple_count = 0;
    motor->apple_bug_count = 0;

    Lgr->pal->set();
}

// lejatszo ezt beallitja: 0 -> nem kell kiirni, 1 -> A eloszor, 2 -> B
// csak akkor A vagy B eloszor, ha mindketten meghaltak, tehat
// menu_level menu-nek ezt mar feldolgozta elore:
int MeghalteloszorAB = 0;

// Ha sikeres szint, ide teszi be hogy ki ment ki:
int Aerintetteviragot = 0;

int Masodikmenet = 0;

// Idot adja vissza szazadmasodpercben:
long lejatszo(const char* filenev, CameraMode cameramode) {
  return -1;
}

// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY
// REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY REPLAY

// char Ccc[] = "Cs1_2";

static void reverse_events(motorst* pmot, recorder* prec, double time) {
    WavEvent wav;
    double vol;
    int obj;
    while (prec->recall_event_reverse(time, &wav, &vol, &obj)) {
        if (obj >= 0 && Ptop->objects[obj] && Ptop->objects[obj]->type == object::Type::Food) {
            Ptop->objects[obj]->active = true;
            pmot->apple_count--;
        }
    }
}

// During rewind, compute animation state from the recorder's event list
// instead of relying on the forward-only state machine.
static void rewind_override_animations(valtozok* pvalt, motorst* pmot, recorder* prec,
                                       double eddig) {
    double turn_time = prec->find_last_turn_frame_time(eddig);

    pvalt->baljobbv_f.eddighatra = pmot->flipped_bike;
    pvalt->baljobbv_f.ucsoforgas = turn_time;
    pvalt->baljobbv_f.ucsoford = turn_time;
    baljobbelintez(&pvalt->baljobbv_f, nullptr, eddig, pmot->flipped_bike);

    pvalt->baljobbv_h.eddighatra = pmot->flipped_camera;
    pvalt->baljobbv_h.ucsoforgas = eddig - 1000.0;
    pvalt->baljobbv_h.ucsoford = eddig - 1000.0;
    pvalt->baljobbv_h.forgas = 1.0;
    pvalt->baljobbv_h.baljobb = pmot->flipped_camera ? 0.0 : 1.0;

    bool is_right_volt = true;
    double volt_time = prec->find_last_volt_time(eddig, &is_right_volt);
    pvalt->utolsougras = volt_time;
    pvalt->ugras1volt = is_right_volt ? 1 : 0;
    pvalt->ugrasnagysag = 1.0 - (eddig - pvalt->utolsougras) / VoltDelay;
    pvalt->ugrasnagysag = std::max<double>(pvalt->ugrasnagysag, 0);
}

// Hamissal ter vissza, ha nincsen mar tobb:
static bool replaymag(motorst* pmot, player_keys* popciok, valtozok* pvalt, recorder* prec,
                      double eddig, viewtimest* pvt, int* pmasikshow) {

    toggleresz(popciok, pvalt, &pvt->viewkinreplay, &pvt->timekinreplay, pmasikshow);

    // recall koordokat es Hatra-t is beallitja:
    bool visszaad = false; // prec->recall_frame(pmot, eddig, &pvalt->inf);
    set_head_position(pmot); // mivel nincs leptet ami kiszamolna
    // HANG ELINTEZES:
    WavEvent wavindex;
    double hangero;
    int objszam;
    while (prec->recall_event(eddig, &wavindex, &hangero, &objszam)) {
        if (objszam >= 0) {
            int prev_apple_count = pmot->apple_count;
            spritefeldolgoz(objszam, &pmot->apple_count, pmot);
            if (prev_apple_count < pmot->apple_count) {
                pmot->last_apple_time = eddig * TimeToCentiseconds;
            }
        } else {
            start_wav(wavindex, hangero);
            if (wavindex == WavEvent::RightVolt) {
                pvalt->ugras1volt = 1;
                pvalt->utolsougras = eddig;
            }
            if (wavindex == WavEvent::LeftVolt) {
                pvalt->ugras1volt = 0;
                pvalt->utolsougras = eddig;
            }
        }
    }

    // Hatra elintezes:
    baljobbelintez(&pvalt->baljobbv_f, nullptr, eddig, pmot->flipped_bike);

    if (pmot->gravity_direction == MotorGravity::Up) {
        pmot->flipped_camera = !pmot->flipped_bike;
    } else {
        pmot->flipped_camera = pmot->flipped_bike;
    }
    baljobbelintez(&pvalt->baljobbv_h, nullptr, eddig, pmot->flipped_camera);

    // Kirajzolashoz kellenek:
    pvalt->ugrasnagysag = 1.0 - (eddig - pvalt->utolsougras) / VoltDelay;
    pvalt->ugrasnagysag = std::max<double>(pvalt->ugrasnagysag, 0);

    return visszaad;
}

static int Elozoshowkep1 = 1, Elozoshowkep2 = 1;

// Termeszetes befejezes eseten 0-t ad vissza:
long lejatszo_r(const char* filenev, int showkepmarad) {
}

// Sets the path of VideoOutputDirectory to "./renders/{replay_stem}/"
void setup_render_directory(const std::string& replay_filename) {
    std::filesystem::path p(replay_filename);
    std::string stem = p.stem().string();
    std::filesystem::path out_dir = std::filesystem::path("renders") / stem;
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec) {
        internal_error("Failed to create output directory for replay render:\n" + ec.message());
    }

    VideoOutputDirectory = out_dir.string();
}

void render_replay(const char* level_filename) {
    Single = !MultiplayerRec;
    Tag = Rec1->flagtag();
    //setup_gameloop(level_filename);

    valtozok valt1;
    memset(&valt1, 0, sizeof(valt1));
    valtozok valt2;
    memset(&valt2, 0, sizeof(valt2));

    valt1.baljobbv_f.ucsoford = valt2.baljobbv_f.ucsoford = -1000.0;
    valt1.baljobbv_f.ucsoforgas = valt2.baljobbv_f.ucsoforgas = -1000.0;
    valt1.baljobbv_h.ucsoford = valt2.baljobbv_h.ucsoford = -1000.0;
    valt1.baljobbv_h.ucsoforgas = valt2.baljobbv_h.ucsoforgas = -1000.0;
    valt1.utolsougras = valt2.utolsougras = -100.0;
    valt1.showkep = 1;
    valt2.showkep = 1;

    camera current_camera;
    current_camera.mode = CameraMode::Normal;

    VideoRecordingMode = true;
    VideoFrameIndex = 0;

    while (true) {
        handle_events();
        if (is_game_key_down(DIK_ESCAPE)) {
            break;
        }

        double eddig = (double)VideoFrameIndex * (STOPWATCH_MULTIPLIER * 1000.0 * 0.0024) /
                       EolSettings->recording_fps();

        int befejezte1 =
            !replaymag(Motor1, &State->keys1, &valt1, Rec1, eddig, &Viewtime1, &valt2.showkep);
        int befejezte2 = 0;
        if (!Single) {
            befejezte2 =
                !replaymag(Motor2, &State->keys2, &valt2, Rec2, eddig, &Viewtime2, &valt1.showkep);
        }

        if ((Single && befejezte1) || (!Single && befejezte1 && befejezte2)) {
            break;
        }

        if (!Single) {
            flagtag_replay(eddig);
        }

        //kirajzol320(eddig, &valt1, &valt2, Viewtime1.viewkinreplay, Viewtime1.timekinreplay,
        //            Viewtime2.viewkinreplay, Viewtime2.timekinreplay, current_camera, Motor1);

        VideoFrameIndex++;
    }

    VideoRecordingMode = false;
    Ptop->unflip_objects();
}
