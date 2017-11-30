#pragma GCC optimize("O3,omit-frame-pointer,inline")

#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <stdint.h>
#include <assert.h>
#include <functional>
#include <algorithm>
#include <set>

#define LARGEUR 11
#define HAUTEUR 13
#define PROFONDEUR_MONTE_CARLO 7

#define SOL -1
#define CAISSE 0
#define CAISSE_TYPE_1 1
#define CAISSE_TYPE_2 2
#define MUR 3

#define JOUEUR 0
#define BOMBE 1
#define OBJET 2

#define EXTRA_PORTEE 1
#define EXTRA_BOMBE 2

#define HAUT 0
#define DROITE 1
#define BAS 2
#define GAUCHE 3
#define STOP 4

#define POWER 3

//#define TEST
#define RUN

// World defines {
#define tickBombs(timeline) ( timeline = ( ++timeline == 8)?0:timeline)
#define bitUp(mask, p) ((mask) & (unary_masks[p]))
#define bitDown(mask, p) (!bitUp(mask,p))
#define upBit(mask, p) ((mask) |= (unary_masks[p]))
#define removeItems(items, mask) ((items) &= ~(mask))
#define addItems(items, mask) ((items) |= (mask))
#define bitcount(mask) (__builtin_popcountll((mask) & 0xFFFFFFFFFFFFFFFFull) + __builtin_popcountll((mask) >> 64))
// }

using namespace std;
using namespace chrono;

high_resolution_clock::time_point global_time;

short myId;
short nbPlayers;
int turn;
bool caissesLeft;
bool poseBombeMode;
int global_my_param2;
int global_my_param1;
int X[113];
int Y[113];
int g[143];
int global_drop_type[13][11];
unsigned __int128 unary_masks[128];

#define F(x,y) (x+y*13)
#define FoG(x,y) (g[F(x,y)])

void initBijection()
{
    for (int i = 0; i < 128; i++) {
        unary_masks[i] = static_cast<unsigned __int128>(1) << i;
    }
    int curr_id = 0;
    for (int x = 0; x < 13; ++x)
    {
        for (int y = 0; y < 11; ++y)
        {
            if (x%2==1 && y%2==1)
            {
                g[x+y*13] = -1;
            }
            else
            {
                g[x+y*13] = curr_id;
                X[curr_id] = x;
                Y[curr_id] = y;
                curr_id++;
            }
        }
    }
}


uint32_t myrand()
{
    static uint32_t x = 7878, y = 2, z = 3, w = 4;
    uint32_t t = x;
    t ^= t << 11;
    t ^= t >> 8;
    x = y;
    y = z;
    z = w;
    w ^= w >> 19;
    w ^= t;
    return w;
}

typedef struct Decision
{
    Decision() {}
    Decision(short d, bool b):direction(d),bomb(b) {}
    short direction;
    bool bomb;
    void show()
    {
        cerr << direction << " " << bomb << endl;
    }
    void randomize()
    {
        int rd = myrand()%8;
        if (rd == 0) direction = STOP;
        else direction = myrand()%4;
        bomb = ((myrand()%2)==0);
    }
} Decision;

Decision droiteBomb = Decision(DROITE,true);
Decision droiteMove = Decision(DROITE,false);
Decision gaucheBomb = Decision(GAUCHE,true);
Decision gaucheMove = Decision(GAUCHE,false);
Decision hautBomb = Decision(HAUT,true);
Decision hautMove = Decision(HAUT,false);
Decision basBomb = Decision(BAS,true);
Decision basMove = Decision(BAS,false);
Decision stayBomb = Decision(STOP,true);
Decision stayMove = Decision(STOP,false);
Decision decisions[10];


unsigned __int128 global_caisses_type_1;
unsigned __int128 global_caisses_type_2;

typedef struct World
{
    // Transfer in copy data
    unsigned __int128 bombs=0;
    unsigned __int128 bombs_by_owner[4];
    unsigned __int128 bombs_by_range[11];
    uint_fast32_t bombs_by_timeline[8];
    int bomb_timeline=0;
    int px;
    int py;
    unsigned __int128 objets_type_1=0;
    unsigned __int128 objets_type_2=0;
    unsigned __int128 caisses=0;
    float score;
    int my_param1;
    int my_param2;
    int profondeur;
    bool applied = false;
    Decision first_applied;

    // No transfer in copy data
    unsigned __int128 removeBombMask;
    unsigned __int128 removeObjetsMask;
    unsigned __int128 removeCaissesMask;
    unsigned __int128 already_taken;
    unsigned __int128 add_type_1_mask;
    unsigned __int128 add_type_2_mask;

    void show() {
        cerr << "Show world: (score " << score << ")\n";
        for (int y = 0; y < 11; y++) {
            for (int x = 0; x < 13; x++) {
                if (x&1 && y&1) {
                    cerr << "X";
                }
                else if (bitUp(caisses, FoG(x,y))) {
                    cerr << "O";
                } else {
                    cerr << ".";
                }
            }
            cerr << endl;
        }
        cerr << "Player in " << px << " " << py << endl;

        for (int i = 0; i < 113; i++) {
            if (bitUp(bombs, i)) {
                cerr << "Bomb in " << X[i] << " " << Y[i] << endl;
            }
        }
        cerr << endl;
    }

    World() {}

    __attribute__((optimize("unroll-loops")))
    World(const struct World& world)
    {
        bombs = world.bombs;
        for (int i = 0; i < 4; i++)
        {
            bombs_by_owner[i] = world.bombs_by_owner[i];
        }
        for (int i = 0; i < 11; i++)
        {
            bombs_by_range[i] = world.bombs_by_range[i];
        }
        px = world.px;
        py = world.py;
        my_param1 = world.my_param1;
        my_param2 = world.my_param2;
        objets_type_1 = world.objets_type_1;
        objets_type_2 = world.objets_type_2;
        bomb_timeline = world.bomb_timeline;
        profondeur = world.profondeur;
        applied = world.applied;
        first_applied = world.first_applied;
        for (int i = 0; i < 8; i++)
        {
            bombs_by_timeline[i] = world.bombs_by_timeline[i];
        }
        caisses = world.caisses;
        score = world.score;
    }

    __attribute__((optimize("unroll-loops")))
    void explodeBomb(int pbomb)
    {
        upBit(removeBombMask,pbomb);
        int bx = X[pbomb];
        int by = Y[pbomb];

        if (bx == px && by == py)
        {
            score -= 50;
        }

        int param2 = -1;
        for (int i = 0; i < 11; i++)
        {
            if (bitUp(bombs_by_range[i],pbomb))
            {
                param2 = i+3;
                break;
            }
        }
        assert(param2 != -1);

        for (int k = 0; k < 4; k++)
        {
            for (int ip = 1; ip < param2; ip++)
            {
                int x = bx + ip*(k==0) - ip*(k==1);
                int y = by + ip*(k==2) - ip*(k==3);
                if (x >= HAUTEUR || x < 0 || y >= LARGEUR || y < 0)
                {
                    break;
                }
                int pm = FoG(x,y);
                if (pm == -1)
                {
                    break;
                }
                if (x&1 && y&1) {
                    cout << "WTF";
                }
                else if (bitUp(caisses, pm))
                {
                    if (bitUp(bombs_by_owner[myId], pbomb))
                    {
                        if (!bitUp(already_taken,pm))
                        {
                            upBit(already_taken,pm);
                            score++;
                        }
                    }
                    upBit(removeCaissesMask, pm);
                    if (global_drop_type[x][y] == CAISSE_TYPE_1)
                        upBit(add_type_1_mask,pm);
                    else if (global_drop_type[x][y] == CAISSE_TYPE_2)
                        upBit(add_type_2_mask,pm);
                    break;
                }
                else
                {
                    if (x == px && y == py)
                    {
                        score -= 50;
                    }
                    if (bitUp(bombs, pm))
                    {
                        if (bitDown(removeBombMask,pm))
                        {
                            explodeBomb(pm);
                        }
                        break;
                    }
                    if (bitUp(objets_type_1,pm))
                    {
                        upBit(removeObjetsMask,pm);
                        break;
                    }
                    else if (bitUp(objets_type_2,pm))
                    {
                        upBit(removeObjetsMask,pm);
                        break;
                    }
                }
            }
        }
    }


    __attribute__((optimize("unroll-loops")))
    inline void explodeBombs()
    {
        for (int i = 0; i < 4; i++)
        {
            uint_fast8_t pom = (bombs_by_timeline[bomb_timeline] >> (i<<3)) & 0xFF;
            if (pom != 0xFF)
            {
                if (bitUp(bombs,pom))
                {
                    explodeBomb(pom);
                }
            }
        }
    }


    bool isMoveable(int x, int y)
    {
        if (x&1 && y&1)
            return false;
        if (x < 0 || y < 0 || x >= 13 || y >= 11)
            return false;
        int pm = FoG(x,y);
        if (bitUp(caisses,pm))
            return false;
        if (bitUp(bombs,pm))
            return false;
        return true;
    }

    void moveIfAble(int x, int y)
    {
        if (!isMoveable(x,y))
            return;
        int pm = FoG(x,y);
        if (bitUp(objets_type_1,pm))
        {
            my_param2++;
            score+=0.1;
        }
        else if (bitUp(objets_type_2,pm)) {
            my_param1++;
            score+=0.3;
        }
        px = x;
        py = y;
    }

    __attribute__((optimize("unroll-loops")))
    void applyDecision(Decision decision)
    {
        if (!applied) {
            applied = true;
            first_applied = decision;
        }
        profondeur++;
        removeBombMask = 0;
        removeObjetsMask = 0;
        removeCaissesMask = 0;
        already_taken = 0;
        add_type_1_mask = 0;
        add_type_2_mask = 0;

        // 1. Au début d'un tour, toutes les bombes en jeu voient leur compte à rebours décrémenté de 1.
        tickBombs(bomb_timeline);

        // 2. Tout compte à rebours arrivant à 0 provoquera l'explosion immédiate de la bombe, avant que les joueurs ne bougent.
        // 3. Toute bombe prise dans une explosion est traitée comme si elle avait explosé au même moment.
        // 4. Les explosions ne se propagent pas après un obstacle (objet, caisse, bombe, mur), mais sont effectives sur la case de l'obstacle.
        // 5. Un obstacle peut arrêter plusieurs explosions au même tour.
        // 6. Une fois que toutes les explosions sont terminées, les caisses affectées sont comptabilisées. Cela implique que plusieurs joueurs peuvent recevoir des points pour la même caisse.
        explodeBombs();

        int ox = px;
        int oy = py;

        // 7. Puis, les joueurs réalisent leurs actions, simultanément.
        if (decision.direction == GAUCHE)
        {
            moveIfAble(px-1, py);
        }
        if (decision.direction == DROITE)
        {
            moveIfAble(px+1, py);
        }
        if (decision.direction == HAUT)
        {
            moveIfAble(px, py-1);
        }
        if (decision.direction == BAS)
        {
            moveIfAble(px, py+1);
        }

        // 8. Les nouveaux objets apparaissent, les caisses et objets détruits dissparaissent.
        removeItems(caisses,removeCaissesMask);
        removeItems(objets_type_1,removeObjetsMask);
        removeItems(objets_type_2,removeObjetsMask);
        removeItems(bombs,removeBombMask);
        for (int i = 0; i < 4; i++)
        {
            removeItems(bombs_by_owner[i],removeBombMask);
        }
        for (int i = 0; i < 11; i++)
        {
            removeItems(bombs_by_range[i],removeBombMask);
        }
        bombs_by_timeline[bomb_timeline] = 0xFFFFFFFF;
        addItems(objets_type_1,add_type_1_mask);
        addItems(objets_type_2,add_type_2_mask);


        // 9. Une bombe placée par un joueur apparaît à la fin du tour.
        if (decision.bomb)
        {
            if (my_param1 > 0)
            {
                score += 0.05;
                my_param1--;
                int pm = FoG(ox,oy);
                if (bitDown(bombs,pm))
                {
                    upBit(bombs,pm);
                    upBit(bombs_by_owner[myId],pm);
                    int rangeid = my_param2 - 3;
                    if (rangeid <= 10)
                        upBit(bombs_by_range[rangeid],pm);
                    else
                        upBit(bombs_by_range[10],pm);
                    removeItems(bombs_by_timeline[bomb_timeline], (0xFF<<(myId*8)));
                    addItems(bombs_by_timeline[bomb_timeline], (pm << (myId*8)));
                }
            }
        }
    }

    inline void hash_combine(std::size_t& seed, unsigned long long v)
    {
        std::hash<unsigned long long> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }


    size_t get_hash() {
        std::hash<unsigned long long> hasher;
        std::size_t seed;
        seed = hasher(bombs>>64);
        hash_combine(seed, bombs&0xFFFFFFFFFFFFFFFFull);
        hash_combine(seed, px);
        hash_combine(seed, py);
        hash_combine(seed, my_param1);
        hash_combine(seed, my_param2);
        for (int i = 0; i < 8; i++) {
            hash_combine(seed, bombs_by_timeline[i]);
        }
        hash_combine(seed, caisses>>64);
        hash_combine(seed, caisses&0xFFFFFFFFFFFFFFFFull);
        hash_combine(seed, objets_type_1>>64);
        hash_combine(seed, objets_type_1&0xFFFFFFFFFFFFFFFFull);
        hash_combine(seed, objets_type_2>>64);
        hash_combine(seed, objets_type_2&0xFFFFFFFFFFFFFFFFull);
        return seed;
    }

    bool can_die_in_2() {
        return false; // Reprendre ici
    }

    int eval() {
        int base = score;
        if (can_die_in_2()) {
            base -= 50;
        }
        return base;
    }

    void BFS() ;


} World;


bool bestscore (World& w1, World& w2) {
    //return w1.eval() > w2.eval();
    return w1.score > w2.score;
}

// World
    void World::BFS() {
        int ix = px, iy = py;
        int N = 400;
        applied = false;

        vector<World> worlds = {*this};

        for (int depth=1; ;++depth) {

            set<size_t> nw_hash;
            vector<World> nextworlds;
            for (const World& w : worlds) {
                for (const Decision& action : decisions) {
                    World ww(w);
                    ww.applyDecision(action);
                    if (ww.score >= 0) {
                        size_t has = ww.get_hash();
                        if (!nw_hash.count(has)) {
                            nextworlds.push_back(std::move(ww));
                            nw_hash.insert(has);
                        }
                    }
                }
            }

            worlds.clear();
            if (nextworlds.size() < N) {
                sort(nextworlds.begin(), nextworlds.end(), bestscore);
                worlds = std::move(nextworlds);
            } else {
                sort(nextworlds.begin(), nextworlds.end(), bestscore);
                for (int i = 0; i < N; i++) {
                    worlds.push_back(std::move(nextworlds[i]));
                }
            }
            /*if (depth <= 100) {
                cerr << "nb dec " << worlds.size();
                if (worlds.size() > 0)
                    cerr << " (best " << worlds[0].score << ") "
                    << "(worst " << worlds[worlds.size()-1].score << ") ";
                cerr << endl;
            }*/



             if (duration_cast<std::chrono::duration<double>>(high_resolution_clock::now() - global_time).count() > 0.090) {
                 cerr << "DEPTH " << depth << endl;
                 break;
             }
        }

        int best = -9999;
        Decision dec = stayMove;
        for (const World& w : worlds) {
            if (w.score > best) {
                best = w.score;
                dec = w.first_applied;
            }
        }
        cerr << "Best score at " << best << endl;
        int p1s = my_param1;
        this->applyDecision(dec);
        this->show();

        if (dec.bomb && p1s > 0)
        {
            cout << "BOMB ";
        }
        else
        {
            cout << "MOVE ";
        }
        switch (dec.direction)
        {
        case STOP:
            cout << ix << " " << iy << " STAY";
            break;
        case DROITE:
            cout << ix+1 << " " << iy << " DROITE";
            break;
        case GAUCHE:
            cout << ix-1 << " " << iy << " GAUCHE";
            break;
        case HAUT:
            cout << ix << " " << iy-1 << " HAUT";
            break;
        case BAS:
            cout << ix << " " << iy+1 << " BAS";
            break;
        default:
            cout << "WTF " << dec.direction << endl;
            break;

        } cout << endl;

    }
// World


World mainWorld;


void run()
{
    int countB = 0;
    Decision best;
    int best_score = -1;

    while (duration_cast<std::chrono::duration<double>>(high_resolution_clock::now() - global_time).count() < 0.095)
    {
        float sc = 0;
        countB++;
        World newWorld(mainWorld);

        Decision first = decisions[myrand()%10];
        newWorld.applyDecision(first);
        sc += newWorld.score;
        for (int i = 0; i < 6; i++) {
            newWorld.applyDecision(decisions[myrand()%10]);
            sc += newWorld.score * pow(0.95, i+1);
        }

        for(int i = 0; i < 8; i++) {
            tickBombs(newWorld.bomb_timeline);
            newWorld.explodeBombs();
            sc += newWorld.score * pow(0.95, 8);
        }

        if (sc > best_score) {
            best_score = sc;
            best = first;
        }
    }
    cerr << countB << " evaluations" << endl;
    cerr << "best is evaluated at " << best_score << endl;

    if (best.bomb && mainWorld.my_param1 > 0)
    {
        cout << "BOMB ";
    }
    else
    {
        cout << "MOVE ";
    }
    switch (best.direction)
    {
    case STOP:
        cout << mainWorld.px << " " << mainWorld.py << " STAY";
        break;
    case DROITE:
        cout << mainWorld.px+1 << " " << mainWorld.py << " DROITE";
        break;
    case GAUCHE:
        cout << mainWorld.px-1 << " " << mainWorld.py << " GAUCHE";
        break;
    case HAUT:
        cout << mainWorld.px << " " << mainWorld.py-1 << " HAUT";
        break;
    case BAS:
        cout << mainWorld.px << " " << mainWorld.py+1 << " BAS";
        break;
    default:
        cout << "WTF " << best.direction << endl;
        break;

    } cout << endl;
}

bool saveYourself()
{
    return false;
}

#ifdef TEST

std::ostream&
operator<<( std::ostream& dest, unsigned __int128 value )
{
    std::ostream::sentry s( dest );
    if ( s ) {
        __uint128_t tmp = value < 0 ? -value : value;
        char buffer[ 128 ];
        char* d = std::end( buffer );
        do
        {
            -- d;
            *d = "0123456789"[ tmp % 10 ];
            tmp /= 10;
        } while ( tmp != 0 );
        if ( value < 0 ) {
            -- d;
            *d = '-';
        }
        int len = std::end( buffer ) - d;
        if ( dest.rdbuf()->sputn( d, len ) != len ) {
            dest.setstate( std::ios_base::badbit );
        }
    }
    return dest;
}

int main()
{
    initBijection();
    mainWorld.bombs = 0;
        for (int i = 0; i < 4; i++) {
            mainWorld.bombs_by_owner[i] = 0;
        }
        for (int i = 0; i < 11; i++) {
            mainWorld.bombs_by_range[i] = 0;
        }
        mainWorld.objets_type_1 = 0;
        mainWorld.objets_type_2 = 0;
        mainWorld.caisses = 0;
        for (int i = 0; i < 8; i++)
        {
            mainWorld.bombs_by_timeline[i] = 0xFFFFFFFF;
        }
        mainWorld.bomb_timeline = 7;
        mainWorld.profondeur = 0;

    string grid[11] = {
        "0....0.......",
        ".............",
        "............0",
        ".............",
        "..........0..",
        "......0......",
        "......0......",
        "......0......",
        "...000...0...",
        ".............",
        "0...........0"
    };
    for (int i = 0; i < 11; i++) {
        string row = grid[i];
        for (int j = 0; j < 13; j++) {
            if (row[j] == '.') {}
            else if (row[j] == '0') {
                upBit(mainWorld.caisses,FoG(j,i));
            }
        }
    }
    myId = 0;
    /*mainWorld.bombsAvailable = -4;
    mainWorld.joueurs.push_back(Entity(0, 0, 1, 1, 3));
    mainWorld.bombes.push_back(Entity(0, 12,0, 1, 3));
    mainWorld.bombes.push_back(Entity(0, 5,1, 2, 3));
    mainWorld.bombes.push_back(Entity(0, 1,4, 3, 3));
    mainWorld.bombes.push_back(Entity(0, 3,4, 8, 3));
    mainWorld.bombes.push_back(Entity(0, 6,8, 5, 3));*/

    mainWorld.show();
    mainWorld.applyDecision(Decision(BAS, false));
    mainWorld.show();
    mainWorld.applyDecision(Decision(DROITE, false));
    mainWorld.show();
    mainWorld.applyDecision(Decision(GAUCHE, false));
    mainWorld.show();
    mainWorld.applyDecision(Decision(GAUCHE, false));
    mainWorld.show();
    mainWorld.applyDecision(Decision(HAUT, false));
    mainWorld.show();
    mainWorld.applyDecision(Decision(HAUT, true));
    mainWorld.show();
    mainWorld.applyDecision(Decision(HAUT, false));
    mainWorld.show();
    mainWorld.applyDecision(Decision(BAS, false));
    mainWorld.show();
    mainWorld.applyDecision(Decision(BAS, true));
    mainWorld.show();


}
#endif


#ifdef RUN

int main()
{
    decisions[0] = droiteBomb;
    decisions[1] = droiteMove;
    decisions[2] = gaucheBomb;
    decisions[3] = gaucheMove;
    decisions[4] = hautBomb;
    decisions[5] = hautMove;
    decisions[6] = basBomb;
    decisions[7] = basMove;
    decisions[8] = stayBomb;
    decisions[9] = stayMove;
    initBijection();
    int width;
    int height;
    cin >> width >> height >> myId;
    cin.ignore();
    mainWorld.score = 0;

    // game loop
    while (1)
    {

        mainWorld.bombs = 0;
        for (int i = 0; i < 4; i++) {
            mainWorld.bombs_by_owner[i] = 0;
        }
        for (int i = 0; i < 11; i++) {
            mainWorld.bombs_by_range[i] = 0;
        }
        mainWorld.objets_type_1 = 0;
        mainWorld.objets_type_2 = 0;
        mainWorld.caisses = 0;
        for (int i = 0; i < 8; i++)
        {
            mainWorld.bombs_by_timeline[i] = 0xFFFFFFFF;
        }
        mainWorld.bomb_timeline = 7;
        mainWorld.profondeur = 0;

        for (int i = 0; i < height; i++)
        {
            string row;
            getline(cin, row);
            for (int j = 0; j < row.length(); j++)
            {
                global_drop_type[j][i] = 0;
                if (row[j] == '.') {}
                else if (row[j] == 'X') {}
                else
                {
                    upBit(mainWorld.caisses,FoG(j,i));
                    if (row[j] == '1')
                    {
                        global_drop_type[j][i] = CAISSE_TYPE_1;
                    }
                    else if (row[j] == '2')
                    {
                        global_drop_type[j][i] = CAISSE_TYPE_2;
                    }
                }
            }
        }
        int nbEntities;
        cin >> nbEntities;
        cin.ignore();

        for (int i = 0; i < nbEntities; i++)
        {
            int entityType;
            int owner;
            int x;
            int y;
            int param1;
            int param2;
            cin >> entityType
                >> owner
                >> x
                >> y
                >> param1
                >> param2;
            cin.ignore();

            if (entityType == BOMBE)
            {
                int pm = FoG(x,y);
                upBit(mainWorld.bombs, pm);
                upBit(mainWorld.bombs_by_owner[owner], pm);
                int rangeid = param2-3;
                if (rangeid <= 10)
                    upBit(mainWorld.bombs_by_range[rangeid], pm);
                else
                    upBit(mainWorld.bombs_by_range[10], pm);
                removeItems(mainWorld.bombs_by_timeline[param1-1], (0xFF<<(owner*8)));
                addItems(mainWorld.bombs_by_timeline[param1-1], (pm << (owner*8)));

            }
            else if (entityType == OBJET)
            {
                if (param1 == 1) {
                    upBit(mainWorld.objets_type_1, FoG(x,y));
                } else if (param1 == 2) {
                    upBit(mainWorld.objets_type_2, FoG(x,y));
                } else {
                    cout << "WWWWhat?" << endl;
                }
            }
            else if (entityType == JOUEUR)
            {
                //mainWorld.joueurs[owner] = Entity(owner, x, y, param1, param2);
                if (owner == myId)
                {
                    mainWorld.px = x;
                    mainWorld.py = y;
                    mainWorld.my_param1 = param1;
                    mainWorld.my_param2 = param2;
                }
            }
        }

        global_time = std::chrono::high_resolution_clock::now();
        mainWorld.show();
        mainWorld.BFS();
    }
}

#endif
