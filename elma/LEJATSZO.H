#ifndef LEJATSZO_H
#define LEJATSZO_H

#include "recorder.h"
#include "physics_forces.h"
#include "state.h"
#include <string>

// Jatek modja ket fele lehet:
extern int Single;
extern int Tag; // Van-e fogocska
// extern int Coop;

extern int Legkozment; // kiraj320 ebbol tudja, hogy snapshot-ot keszitsen
extern bool VideoRecordingMode;
extern int VideoFrameIndex;
extern std::string VideoOutputDirectory;

enum class CameraMode { Normal, MapViewer };

struct camera {
    CameraMode mode;
    double x;
    double y;
    double min_x;
    double min_y;
    double max_x;
    double max_y;
};

// Azt a nevet kerik, amit topol::topol kap:
// BestTime beallitasahoz kell nekik:
long lejatszo(const char* filenev, CameraMode cameramode);
long lejatszo_r(const char* filenev, int showkepmarad);

void setup_render_directory(const std::string& replay_filename);
void render_replay(const char* level_filename);

struct baljobbvaltozok {
    int eddighatra;
    double ucsoforgas;
    double ucsoford;
    double baljobb; // kulsoresz szamolja tobbibol
    double forgas;
};

struct valtozok {
    //bike_sound inf;
    //double surlero;
    //double frekvencia;
    double utolsougras;
    int ugras1volt;

    int showkep;

    int hatranyomva;
    //bool one_turn_used;

    double ugrasnagysag;

    baljobbvaltozok baljobbv_f, baljobbv_h;
};

// lejatszo ezt beallitja: 0 -> nem kell kiirni, 1 -> A eloszor, 2 -> B
// csak akkor A vagy B eloszor, ha mindketten meghaltak, tehat
// menu_level menu-nek ezt mar feldolgozta elore:
extern int MeghalteloszorAB;

// Ha sikeres szint, ide teszi be hogy ki ment ki:
extern int Aerintetteviragot;



// Additional exports to support other gameloop

struct viewtimest {
    int viewkinplay, viewkinreplay;
    int timekinplay, timekinreplay;
};



//// lejatszo ezt beallitja: 0 -> nem kell kiirni, 1 -> A eloszor, 2 -> B
//// csak akkor A vagy B eloszor, ha mindketten meghaltak, tehat
//// menu_level menu-nek ezt mar feldolgozta elore:
//int MeghalteloszorAB = 0;
//
//// Ha sikeres szint, ide teszi be hogy ki ment ki:
//int Aerintetteviragot = 0;
//
//int Masodikmenet = 0;

//static viewtimest Viewtime1 = {1, 0, 1, 0};
//static viewtimest Viewtime2 = {1, 0, 1, 0};


void update_freecam(double dt, camera& current_camera);


void belsoresz(motorst* pmot, player_keys* popciok, valtozok* pvalt, recorder* prec,
                      long* pmegvanido, int* pmeghalt, double eddig, double dt);

void kulsoresz(motorst* pmot, player_keys* popciok, valtozok* pvalt, recorder* prec,
                      viewtimest* pvt, double eddig, int* pmasikshow, int meghalt, int keys);

BikeState spritefeldolgoz(int sorszam, int* pkajaszam, motorst* pmot);


void initsoundlejatszoban();
bool handle_console_input();
void setup_gameloop(const char* filenev, motorst* motor);
bool was_game_key_just_pressed(DikScancode code);




#endif
