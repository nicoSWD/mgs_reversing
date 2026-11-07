#include "animal/zako14/zako.h"

#include "enemy/grnad_e.h"
#include "game/vibrate.h"
#include "enemy/asiato.h"
#include "anime/animconv/anime.h"
#include "enemy/l_sight.h"
#include "game/item.h"
#include "okajima/blood.h"
#include "okajima/bullet.h"

extern void *AN_Breath( MATRIX *world );
extern void *AN_Fog( SVECTOR *pos );
extern void *AN_HeadMark( MATRIX* world, int mark );

extern SVECTOR   ZAKO14_PlayerPosition;
extern COMMANDER ZAKO14Command;
extern TOP       ZAKO14TOPCOMMAND;
extern int       ZAKO14_Unknown;
extern int       ZAKO14_GameFlag;
extern int       ZAKO14_PlayerAddress;
extern int       ZAKO14_PlayerMap;

void AN_Sleep( SVECTOR *pos );
void AN_Unknown_800C3B7C( MATRIX *world );

void s14e_800C96AC( Work *work );
void s14e_800CDC08( Work *work );
void s14e_800CB0BC( Work *work );
void s14e_800CB128( Work *work );

int s14e_800C6A34( Work *work );
int s14e_800C6AC0( Work *work );
int s14e_800C6ADC( SVECTOR *vec );
int s14e_800C6B18( Work *work );
int s14e_800C6DB0( Work *work );
int s14e_800C6E08( Work *work );
void s14e_800C6E6C( Work *work );
int s14e_800C6F40( int from, int to );
void s14e_800C6F94( Work *work );
int s14e_800C701C( Work *work );
void s14e_800C712C( Work *work );
int s14e_800C7194( Work *work );
void s14e_800C7398( Work *work, int time );
void s14e_800C7550( Work *work, int time );
void s14e_800C77F8( Work *work, int time );
void s14e_800C7888( Work *work, int time );
void s14e_800C7970( Work *work, int time );
void s14e_800C7978( Work *work, int time );
void s14e_800C7AEC( Work *work, int time );
void s14e_800C7B9C( Work *work, int time );
void s14e_800C7CD0( Work *work, int time );
void s14e_800C7E28( Work *work, int time );
void s14e_800C7F00( Work *work, int time );
void s14e_800C7FA8( Work *work, int time );
void s14e_800C82D0( Work *work );
void s14e_800C8350( Work *work, int time );
void s14e_800C86D4( Work *work, int time );
void s14e_800C8748( Work *work, int time );
void s14e_800C8898( Work *work, int time );
void s14e_800C8A10( Work *work, int time );
void s14e_800C8AD8( Work *work, int time );
void s14e_800C8BE8( Work *work, int time );
void s14e_800C8CF8( Work *work, int time );
void s14e_800C94A8( Work *work, int time );
void s14e_800C95B8( Work *work, int time );
void s14e_800C924C( Work *work, int time );

void s14e_800C94A8( Work *work, int time );
void s14e_800C95B8( Work *work, int time );
void s14e_800C9838( Work *work, int time );
void s14e_800C9904( Work *work, int time );
void s14e_800C99D0( Work *work, int time );
void s14e_800C9A68( Work *work, int time );
void s14e_800C9B00( Work *work, int time );
void s14e_800C9BB4( Work *work, int time );
void s14e_800C9C7C( Work *work, int time );
void s14e_800C9D20( Work *work, int time );
void s14e_800C9DE8( Work *work, int time );
void s14e_800CA000( Work *work, int time );
void s14e_800CA148( Work *work, int time );

void s14e_800CA2E4( Work *work, int unit, int count );
void s14e_800CA354( Work *work );
void s14e_800CA3B8( Work *work );
int  s14e_800C701C( Work *work );
int  s14e_800CA8D8( Work *work, void *func );
void s14e_800CA90C( Work *work );

short ActionTable[] = {
    19, 20, 10, 22, 9, 34, 35, 36,
    45, 13, 12, 26, 15, 14, 16, 27,
    8, 46, 19, 21, 30, 31, 32, 33,
    37, 19, 19, 25, 5, 2, 42, 4,
    39, 40, 6, 7, 38, 3, 41, 17,
    18, 43, 23, 24, 28, 29, 0, 1,
    51, 17, 18, 43, 22,
};
int s14e_dword_800C33BC = 0x00000401;
int s14e_dword_800C33C0 = 0x000004FF;
static SVECTOR s14e_dword_800C33C4 = { -150, 0, 300 };
static SVECTOR BreathPos = { 0, -700, 900 };


int s14e_800C6A34( Work *work )
{
    TARGET *target;

    target = work->target;
    if ( target->damaged & TARGET_CAPTURE )
    {
        if ( target->a_mode == 36 )
        {
            SetMode( work, s14e_800C7FA8 );
        }
        else
        {
            SetMode( work, s14e_800C8350 );
        }

        target->damaged = 0;
        return 1;
    }

    return 0;
}

int s14e_800C6AC0( Work *work )
{
    if (work->target->damaged & 0x20)
    {
        return 1;
    }
    return 0;
}

int s14e_800C6ADC( SVECTOR *vec )
{
    return ABS( vec->vx ) + ABS( vec->vy ) + ABS( vec->vz );
}

int s14e_800C6B18( Work *work )
{
    TARGET *target;

    target = work->target;
    if ( !( target->damaged & TARGET_POWER ) ) return 0;

    switch ( target->a_mode )
    {
    case 3:
        target->force = DG_ZeroVector;
        if ( target->faint <= 0 )
        {
            work->act.field_14 = 1;
            SetMode( work, s14e_800C8CF8 );
        }
        else
        {
            SetMode( work, s14e_800C8BE8 );
        }
        break;
    case 4:
        work->act.field_14 = 0;
        SetMode( work, s14e_800C8CF8 );
        break;
    case 1:
        target->force = DG_ZeroVector;
        if ( target->vital <= 0 || work->param.unknown - target->vital >= 192 )
        {
            work->act.field_14 = 1;
            SetMode( work, s14e_800C8CF8 );
        }
        else
        {
            SetMode( work, s14e_800C8BE8 );
        }
        break;
    case 2:
        if ( s14e_800C6ADC( &target->force ) < 100 )
        {
            work->act.field_14 = 3;
        }
        else
        {
            work->act.field_14 = 2;
        }
        SetMode( work, s14e_800C8CF8 );
        break;
    case 5:
        work->act.field_14 = 3;
        SetMode( work, s14e_800C8CF8 );
        break;
    case 8:
        SetMode( work, s14e_800C86D4 );
        break;
    case 9:
        SetMode( work, s14e_800C8748 );
        break;
    }

    target->damage = 0;
    target->damaged = 0;
    return 1;
}


int s14e_800C6D20( Work *work )
{
    if ( work->control.levels[ 0 ] != 0 )
    {
        work->target->vital = 0;
        SetAction( work, 39, 4 );
        SetMode( work, s14e_800C924C );
        return 1;
    }

    return 0;
}
int s14e_800C6DB0( Work *work )
{
    if ( GM_GameStatus & STATE_STUN )
    {
        SetMode( work, s14e_800C8748 );
        return 1;
    }

    return 0;
}
int s14e_800C6E08( Work *work )
{
    int capture, power, stun;

    power = s14e_800C6B18( work );
    if ( power == 0 )
    {
        capture = s14e_800C6A34( work );
        stun = s14e_800C6DB0( work );
        if ( ( capture | stun ) == 0 )
        {
            return 0;
        }
    }

    work->target->class = TARGET_AVAIL;
    return 1;
}
void s14e_800C6E6C( Work *work )
{
    SVECTOR *mov;

    mov = &work->control.mov;
    setVector( mov, 30000, 30000, 30000 );
    work->param.item = 1;
    work->think3 = 1;
    work->field_9E4 = 0;
    work->think1 = 0;
    work->think2 = 0;
    work->think4 = 0;
    work->count3 = 0;
    work->next_node = work->n_nodes - 1;
    work->target_addr = work->field_BE8;
    work->target_pos = work->start_pos;
    work->vision.range = 512;
    work->vision.length = ZAKO14_EYE_LENGTH;
    work->alert_level = 0;
    work->vision.facedir = 0;
    work->pad.sound = 0;
    work->pad.field_4 = 1;
    work->field_C44 = 0;
    work->target_map = work->start_map;
    work->next_pos = work->start_pos;
    work->last_addr = work->next_addr = work->field_BE8;
}

int s14e_800C6F40( int from, int to )
{
    int diff;

    if ( to < 0 ) return 0;
    if ( from < 0 ) return 0;
    diff = GV_DiffDirS( from, to );
    if ( diff < -896 || diff > 896 ) return 0;
    if ( diff >= 0 ) return 1;
    return 2;
}

void s14e_800C6F94( Work *work )
{
    int h1, h2, ang, rotx;

    h1 = GM_PlayerBody->objs->objs[ 6 ].world.t[ 1 ];
    h2 = work->body.objs->objs[ 6 ].world.t[ 1 ];
    ang = ratan2( work->sn_dis, h1 - h2 ) & 4095;
    rotx = GV_NearExp8( work->adjust[ 2 ].vx, ang - 1024 );
    work->adjust[ 2 ].vx = rotx;
    work->adjust[ 6 ].vx = rotx;

    if ( rotx < 0 )
    {
        work->adjust[ 7 ].vx = rotx * 3;
    }
    else
    {
        work->adjust[ 7 ].vx = rotx * 3 / 2;
    }
}

int s14e_800C701C( Work *work )
{
    SVECTOR svec;
    SVECTOR rp_shift = { -200, 1000, 600 };
    SVECTOR force = { 5, 0, 100 };
    SVECTOR size = { 400, 1000, 400 };
    TARGET *target;

    target = &work->punch;
    GM_SetTarget( target, TARGET_POWER, ENEMY_SIDE, &size );

    DG_SetPos2( &work->control.mov, &work->control.rot );
    DG_RotVector( &force, &svec, 1 );
    GM_SetPowerTarget( target, POWER_ONCE, 2, 32, 1, &svec );

    DG_PutVector( &rp_shift, &target->center, 1 );
    return GM_PowerTarget( target );
}

const char s14e_dword_800DC9E4[] = " vx=%d vy=%d vz=%d \n";
const char s14e_dword_800DC9FC[] = {0x0, 0x0, 0x0, 0x0};
const int s14e_dword_800DCA00 = 0x800C8DA4;
const int s14e_dword_800DCA04 = 0x800C8DF4;
const int s14e_dword_800DCA08 = 0x800C8EBC;
const int s14e_dword_800DCA0C = 0x800C8E78;
const int s14e_dword_800DCA10 = 0x800C8F00;
const int s14e_dword_800DCA14 = 0x800C8F34;
const int s14e_dword_800DCA18 = 0x800C8F8C;
const int s14e_dword_800DCA1C = 0x800C8FD4;
const int s14e_dword_800DCA20 = 0x800C9014;
const int s14e_dword_800DCA24 = 0x800C8FF4;
const int s14e_dword_800DCA28 = 0x800C9040;
const int s14e_dword_800DCA2C = 0x800C9040;

static void s14e_800CA538( Work *work );
static void s14e_800CA5D0( Work *work );
static void s14e_800CA718( Work *work );
static void s14e_800CA754( Work *work );

static SVECTOR BloodPos = {0, 0, 100};
static SVECTOR BloodRot = {-1024, 0, 0};

static SVECTOR FogPos = {0, 0, 100};
static SVECTOR FogRot = {-1024, 0, 0};

static char *ItemText[] = {
    "RATION",
    "SOCOM/BULLET * 12",
    "FA-MAS/BULLET * 25",
};

static void *PutChars[] = {
    s14e_800CA538,
    s14e_800CA5D0,
    s14e_800CA718,
    s14e_800CA754,
};

static SVECTOR BulletPos = {5, -500, 80};

void s14e_800C712C( Work *work )
{
    TARGET *target;

    target = &work->attack;
    GM_SetPowerTarget( target, POWER_CONST | POWER_EXPLODE, 5, 0, 3, &ZAKO14_ATTACK_FORCE );
    GM_MoveTarget( target, &work->control.mov );
    GM_PowerTarget( target );
}

int s14e_800C7194( Work *work )
{
    int press;

    press = work->pad.press;

    if ( press & 0x1 )
    {
        SetMode2( work, s14e_800C9838 );
        return 0;
    }

    if ( press & 0x2 )
    {
        SetMode2( work, s14e_800C9904 );
        return 0;
    }

    if ( press & 0x4 )
    {
        SetMode2( work, s14e_800C99D0 );
        return 0;
    }

    if ( press & 0x2000 )
    {
        SetMode2( work, s14e_800C9A68 );
        return 0;
    }

    if ( press & 0x40 )
    {
        SetMode2( work, s14e_800C9B00 );
        return 0;
    }

    if ( press & 0x80 )
    {
        SetMode2( work, s14e_800C9BB4 );
        return 0;
    }

    if ( press & 0x400 )
    {
        SetMode2( work, s14e_800C9C7C );
        return 0;
    }

    if ( press & 0x800 )
    {
        SetMode2( work, s14e_800C9D20 );
        return 0;
    }

    if ( press & 0x200 )
    {
        SetMode2( work, s14e_800C9DE8 );
        return 0;
    }

    if ( press & 0x800000 )
    {
        SetMode2( work, s14e_800CA000 );
        return 0;
    }

    if ( press & 0x1000000 )
    {
        SetMode2( work, s14e_800CA148 );
        return 0;
    }

    if ( press & 0x100 )
    {
        SetMode( work, s14e_800C77F8 );
        return 1;
    }

    if ( press & 0x20 )
    {
        SetMode( work, s14e_800C7888 );
        return 1;
    }

    if ( press & 0x400000 )
    {
        SetMode( work, s14e_800C7970 );
        return 1;
    }

    if ( press & 0x30000 )
    {
        SetMode( work, s14e_800C7978 );
        work->adjust[ 2 ].vx = 0;
        return 1;
    }

    return 0;
}

void s14e_800C7398( Work *work, int time )
{
    work->target->class |= TARGET_FLAG;
    work->act_status |= 0x1;
    if ( ZAKO14Command.alert_mode == 0 ) work->target->class |= TARGET_C4;
    work->vision.length = ZAKO14_EYE_LENGTH;

    if ( time == 0 ) SetAction( work, 0, 4 );

    if ( s14e_800C6E08( work ) || s14e_800C6D20( work ) || s14e_800C7194( work ) )
    {
        UnsetMode( work );
        return;
    }

    if ( work->pad.dir >= 0 )
    {
        if ( GV_DiffDirAbs( work->control.rot.vy, work->pad.dir ) < 256 )
        {
            if ( work->pad.field_4 & 1 )
            {
                SetAction( work, 2, 4 );
            }
            else
            {
                SetAction( work, 1, 4 );
            }

            SetMode( work, s14e_800C7550 );
        }
        else
        {
            work->control.turn.vy = work->pad.dir;
        }
    }

    work->vision.facedir = work->control.rot.vy;
}

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7550.s")

void s14e_800C77F8( Work *work, int time )
{
    if ( time == 0 )
    {
        work->act.field_14 = 3;
        SetAction( work, 35, 4 );
    }

    if ( !s14e_800C6E08( work ) && work->body.is_end )
    {
        work->pad.field_8 = 0;
        SetMode( work, s14e_800C8898 );
    }
}

void s14e_800C7888( Work *work, int time )
{
    CONTROL *ctrl;

    ctrl = &work->control;

    work->target->class |= TARGET_FLAG;
    work->vision.length = ZAKO14_EYE_LENGTH;

    if ( time == 0 ) SetAction( work, 16, 4 );

    if ( !s14e_800C6E08( work ) )
    {
        if ( work->body.is_end || !( work->pad.press & 0x20 ) )
        {
            SetMode( work, s14e_800C7398 );
        }
        else
        {
            ctrl->turn.vy = work->sn_dir;
            ctrl->step.vx = 0;
            ctrl->step.vz = 0;
            work->vision.facedir = work->control.rot.vy;
        }
    }
}


void s14e_800C7970( Work *work, int time )
{
    /* do nothing */
}

void s14e_800C7978( Work *work, int time )
{
    int press;

    work->target->class |= TARGET_FLAG;
    work->vision.length = ZAKO14_EYE_LENGTH;

    press = work->pad.press;

    if ( time == 0 )
    {
        if ( press & 0x10000 )
        {
            SetAction( work, 3, 4 );
        }
        else
        {
            SetAction( work, 5, 4 );
        }

        GM_ConfigMotionAdjust( &work->body, work->adjust );
    }

    s14e_800C6F94( work );
    if ( s14e_800C6E08( work ) ) return;

    if ( !( press & 0x30000 ) )
    {
        SetMode( work, s14e_800C7398 );
        return;
    }

    if ( press & 0x40000 )
    {
        SetModeNoAdjust( work, s14e_800C7AEC );
        return;
    }

    if ( press & 0x80000 )
    {
        SetMode( work, s14e_800C7B9C );
        return;
    }

    if ( press & 0x100000 )
    {
        SetModeNoAdjust( work, s14e_800C7CD0 );
        return;
    }

    if ( press & 0x200000 )
    {
        SetModeNoAdjust( work, s14e_800C7E28 );
        return;
    }

    work->control.turn.vy = work->pad.dir;
    work->vision.facedir = work->control.rot.vy;
}

void s14e_800C7AEC( Work *work, int time )
{
    work->target->class |= TARGET_FLAG;
    work->vision.length = ZAKO14_EYE_LENGTH;

    if ( time == 0 )
    {
        SetAction( work, 4, 0 );
        s14e_800CA890( work, 3 );
        GM_ConfigMotionAdjust( &work->body, work->adjust );
    }

    s14e_800C6F94( work );
    if ( !s14e_800C6E08( work ) ) SetModeNoAdjust( work, s14e_800C7978 );
}

void s14e_800C7B9C( Work *work, int time )
{
    SVECTOR *rot;

    work->target->class |= TARGET_FLAG;
    work->vision.length = ZAKO14_EYE_LENGTH;

    if ( time == 0 )
    {
        SetAction( work, 5, 4 );
        work->control.turn.vy = work->pad.dir;
    }

    if ( s14e_800C6E08( work ) ) return;
    if ( time < 16 ) return;

    rot = &work->control.rot;
    if ( time < 31 )
    {
        rot->vy = ( rot->vy - 32 ) & 4095;
    }
    else if ( time < 61 )
    {
        rot->vy = ( rot->vy + 32 ) & 4095;
    }
    else if ( time < 76 )
    {
        rot->vy = ( rot->vy - 32 ) & 4095;
    }

    if ( !( time & 1 ) )
    {
        GM_SeSet( &work->control.mov, 46 );
        s14e_800CA890( work, 3 );
    }

    if ( time >= 77 )
    {
        SetMode( work, s14e_800C7978 );
    }

    work->control.turn.vy = rot->vy;
}

void s14e_800C7CD0( Work *work, int time )
{
    work->target->class |= TARGET_FLAG;
    work->vision.length = ZAKO14_EYE_LENGTH;

    if ( time == 0 )
    {
        SetAction( work, 7, 4 );
        work->subweapon = NewGrenadeEnemy( &work->control, &work->body, 9, &work->trigger, &GM_PlayerPosition, ENEMY_SIDE );
    }

    if ( time > 4 ) work->trigger |= 1;
    if ( time == 17 ) GM_SeSet( &work->control.mov, SE_PINNUKI );
    if ( time == 45 ) work->trigger |= 4;

    if ( s14e_800C6E08( work ) )
    {
        GV_DestroyActor( work->subweapon );
        return;
    }

    if ( work->body.is_end )
    {
        GV_DestroyActor( work->subweapon );
        SetMode( work, s14e_800C7978 );
    }
}

void s14e_800C7E28( Work *work, int time )
{
    work->target->class |= TARGET_FLAG;
    work->vision.length = ZAKO14_EYE_LENGTH;

    if ( time == 0 )
    {
        SetAction( work, 8, 0 );
    }

    if ( time == 3 && s14e_800C701C( work ) != 0 )
    {
        GM_SeSet( &work->control.mov, SE_HIT );
    }

    work->control.turn.vy = work->sn_dir;

    if ( !s14e_800C6E08( work ) && work->body.is_end )
    {
        SetMode( work, s14e_800C7978 );
    }
}

void s14e_800C7F00( Work *work, int time )
{
    work->vision.length = 0;

    if ( time == 0 )
    {
        SetAction( work, 21, 4 );
    }

    if ( !s14e_800C6E08( work ) )
    {
        work->target->class |= TARGET_FLAG;
        if ( work->body.is_end )
        {
            work->field_8FC = 1;
            SetMode( work, s14e_800C7398 );
        }
    }
}

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7FA8.s")

void s14e_800C82D0( Work *work )
{
    SVECTOR pos;

    work->control.turn = GM_PlayerControl->rot;
    DG_SetPos2( &GM_PlayerPosition, &GM_PlayerControl->rot );
    DG_PutVector( &s14e_dword_800C33C4, &pos, 1 );
    GV_SubVec3( &pos, &work->control.mov, &work->control.step );
}

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C8350.s")

void s14e_800C86D4( Work *work, int time )
{
    if ( time == 0 )
    {
        SetAction( work, 38, 4 );
    }

    if ( work->body.is_end )
    {
        SetMode( work, s14e_800C94A8 );
    }
}

void s14e_800C8748( Work *work, int time )
{
    CONTROL *control;
    int start;

    control = &work->control;

    work->act.field_1E = 0;
    work->vision.length = 0;
    work->act_status |= 0x8;

    if ( time == 0 )
    {
        SetAction( work, 32, 4 );
        work->target->faint = 0;
    }

    if ( !s14e_800C6E08( work ) )
    {
        start = 10;

        if ( time == start )
        {
            work->act.field_14 = 1;
            SetAction( work, 37, 4 );
        }
        else
        {
            work->target->class |= TARGET_FLAG;
        }

        if ( time == ( start + 24 ) )
        {
            GM_SeSet( &control->mov, SE_ENEMY_COLLAPSE );
        }

        if ( time == ( start + 46 ) )
        {
            GM_SeSet( &control->mov, SE_HIT_FLOOR );
        }

        if ( time >= ( start + 50 ) && work->body.is_end )
        {
            SetMode( work, s14e_800C8898 );
        }
    }
}

void s14e_800C8898( Work *work, int time )
{
    int x;

    work->act_status |= 0x20;

    if ( time == 0 )
    {
        if ( work->act.field_14 < 3 )
        {
            if ( work->act.field_14 == 1 )
            {
                SetAction( work, 41, 4 );
            }
            else
            {
                SetAction( work, 39, 4 );
            }
        }
        else
        {
            SetAction( work, 40, 4 );
        }

        x = work->control.mov.vx % 30;
        if ( x < 0 ) x = -x;
        work->field_B5A = x + 10;
    }

    if ( s14e_800C6AC0( work ) )
    {
        SetMode( work, s14e_800C8A10 );
    }
    else if ( work->field_B5A < time )
    {
        if ( work->target->faint <= 0 )
        {
            work->target->faint = work->param.faint;
        }

        SetMode( work, s14e_800C8AD8 );
    }
}

void s14e_800C8A10( Work *work, int time )
{
    if ( time == 0 )
    {
        if ( work->act.field_14 < 3 )
        {
            SetAction( work, 46, 4 );
        }
        else
        {
            SetAction( work, 47, 4 );
        }

        GM_SeSet( &work->control.mov, SE_ITEM_CURSOR );
    }

    if ( time == 4 )
    {
        s14e_800CA3B8( work );
    }

    if ( work->body.is_end )
    {
        SetMode( work, s14e_800C94A8 );
    }
}

void s14e_800C8AD8( Work *work, int time )
{
    work->act.field_1E = 0;
    work->act_status |= 0x8;

    if ( time == 0 )
    {
        if ( work->act.field_14 < 3 )
        {
            if ( work->field_B5A >= 150 )
            {
                SetAction( work, 44, 4 );
            }
            else
            {
                SetAction( work, 42, 4 );
            }
        }
        else
        {
            if ( work->field_B5A >= 150 )
            {
                SetAction( work, 45, 4 );
            }
            else
            {
                SetAction( work, 43, 4 );
            }
        }
    }

    if ( work->body.is_end )
    {
        work->param.unknown = work->target->vital;
        SetMode( work, s14e_800C7398 );
    }
}

void s14e_800C8BE8( Work *work, int time )
{
    TARGET *target;

    work->act.field_1E = 0;
    work->target->class |= TARGET_FLAG;
    work->vision.length = ZAKO14_EYE_LENGTH;
    work->act_status |= 0x8;

    if ( s14e_800C6E08( work ) ) return;

    target = work->target;

    if ( time == 0 )
    {
        SetAction( work, 15, 4 );
        GM_SeSet( &work->control.mov, 141 );

        if ( target->a_mode == 3 )
        {
            GM_SeSet( &work->control.mov, 52 );
            s14e_800CA354( work );
        }
        else
        {
            s14e_800CA2E4( work, 5, 0 );
        }
    }

    if ( work->body.is_end )
    {
        SetMode( work, s14e_800C7398 );
    }
}
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C8CF8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C924C.s")
void s14e_800C94A8( Work *work, int time )
{
    work->act_status |= 0x40;

    if ( time == 0 )
    {
        if ( work->act.field_14 < 3 )
        {
            if ( work->act.field_14 == 1 )
            {
                SetAction( work, 51, 4 );
                GM_ConfigControlAttribute( &work->control, 0 );
                work->alert_level = 0;
            }
            else
            {
                SetAction( work, 49, 4 );
                GM_ConfigControlAttribute( &work->control, 0 );
                work->alert_level = 0;
            }
        }
        else
        {
            SetAction( work, 50, 4 );
            GM_ConfigControlAttribute( &work->control, 0 );
            work->alert_level = 0;
        }
    }

    if ( time == 4 )
    {
        s14e_800CA3B8( work );
    }

    if ( time & 2 )
    {
        work->field_9E4 = 0;
    }
    else
    {
        work->field_9E4 = 1;
    }

    if ( time > 8 ) SetMode( work, s14e_800C95B8 );
}

void s14e_800C95B8( Work *work, int time )
{
    work->act_status |= 0x40;

    if ( time == 0 )
    {
        work->field_9E4 = 0;
        work->control.mov = ZAKO14_NO_POINT;
        ZAKO14Command.watchers[ work->param.index ].field_4 = 1;
        if ( work->field_C44 == 0 ) GM_EnemyKillCount++;
    }

    if ( ZAKO14Command.watchers[ work->param.index ].field_4 == 2 )
    {
        s14e_800C6E6C( work );
        SetMode( work, s14e_800C7398 );
    }
}

void s14e_800C96AC( Work *work )
{
    ACT *act;
    CONTROL *control;
    ACTION action;
    int time;

    act = &work->act;
    control = &work->control;

    work->trigger = 0;
    work->vision.length = 0;
    work->target->class = TARGET_AVAIL;
    work->field_8FC = 0;
    work->act_status = 0;
    work->field_C38 = 0;

    action = work->action;

    control->height = ( work->body.height * work->scale ) / 4096;
    control->hzd_height = control->levels[ 0 ] + 750;

    time = work->time++;

    if ( action == NULL )
    {
        action = s14e_800C7398;
        work->action = s14e_800C7398;
    }

    action( work, time );

    action = work->action2;
    if ( work->action2 != NULL )
    {
        time = work->time2++;
        action( work, time );
    }

    if ( act->field_1E == 0 )
    {
        control->r_sphere = GV_NearExp2( control->r_sphere, act->field_1C );
    }
    else
    {
        control->r_sphere = -1;
    }

    if ( work->target->class & TARGET_POWER )
    {
        work->hom->flag = 1;
    }
    else
    {
        work->hom->flag = 0;
    }

    if ( act->field_04 < 0 && control->grounded ) act->field_04 = 0;

    act->field_04 -= 16;
    control->step.vy = act->field_04;

    if ( work->mark_time > 0 ) work->mark_time--;
}

void s14e_800C9838( Work *work, int time )
{
    if ( time == 0 ) SetAction2( work, 9, 4, 0x3FE );

    if ( !( work->pad.press & 0x1 ) )
    {
        UnsetMode( work );
    }
    else if ( time < 4 )
    {
        work->vision.facedir = ( work->control.rot.vy - time * 256 ) & 4095;
    }
    else
    {
        work->vision.facedir = ( work->control.rot.vy - 1024 ) & 4095;
    }
}

void s14e_800C9904( Work *work, int time )
{
    if ( time == 0 ) SetAction2( work, 10, 4, 0x3FE );

    if ( !( work->pad.press & 0x2 ) )
    {
        UnsetMode( work );
    }
    else if ( time < 4 )
    {
        work->vision.facedir = ( work->control.rot.vy + time * 256 ) & 4095;
    }
    else
    {
        work->vision.facedir = ( work->control.rot.vy + 1024 ) & 4095;
    }
}

void s14e_800C99D0( Work *work, int time )
{
    work->vision.length = 3000;

    if ( time == 0 ) SetAction2( work, 11, 4, 0x3FE );
    if ( !( work->pad.press & 0x4 ) ) UnsetMode( work );
}

void s14e_800C9A68( Work *work, int time )
{
    work->vision.length = 3000;

    if ( time == 0 ) SetAction2( work, 14, 4, 0x3FE );
    if ( !( work->pad.press & 0x2000 ) ) UnsetMode( work );
}

void s14e_800C9B00( Work *work, int time )
{
    work->vision.length = 0;

    if ( time == 0 )
    {
        SetAction2( work, 24, 4, 0x3FE );
        GM_SeSet( &work->control.mov, SE_V_ENE_SNEEZE );
    }

    if ( work->body.time2 != 0 || !( work->pad.press & 0x40 ) )
    {
        work->pad.time = 0;
        UnsetMode( work );
    }
}

void s14e_800C9BB4( Work *work, int time )
{
    work->vision.length = 0;

    if ( time == 0 ) SetAction2( work, 22, 4, 0x3FE );
    if ( time == 60 ) GM_SeSet( &work->control.mov, SE_V_ENE_YAWN );

    if ( work->body.time2 != 0 || !( work->pad.press & 0x80 ) )
    {
        work->pad.time = 0;
        UnsetMode( work );
    }
}

void s14e_800C9C7C( Work *work, int time )
{
    if ( time == 0 ) SetAction2( work, 23, 4, 0x3FE );

    if ( work->body.time2 != 0 || !( work->pad.press & 0x400 ) )
    {
        work->pad.time = 0;
        UnsetMode( work );
    }
}

void s14e_800C9D20( Work *work, int time )
{
    work->vision.length = 0;

    if ( time == 0 ) SetAction2( work, 19, 4, 0x3FE );
    if ( time == 90 ) GM_SeSet( &work->control.mov, SE_V_ENE_YAWN );

    if ( work->body.time2 != 0 || !( work->pad.press & 0x800 ) )
    {
        work->pad.time = 0;
        UnsetMode( work );
    }
}

void s14e_800C9DE8( Work *work, int time )
{
    SVECTOR pos;

    work->vision.length = 0;

    if ( time == 0 ) SetAction2( work, 19, 4, 0x3FE );
    if ( time == 90 ) GM_SeSet( &work->control.mov, SE_V_ENE_YAWN );
    if ( time == 110 ) SetAction2( work, 20, 4, 0x3FE );

    if ( time > 110 )
    {
        work->act_status |= 0x10;

        if ( work->m_ctrl.info2.frame == 30 )
        {
            pos = work->control.mov;
            pos.vy += 500;
            AN_Sleep( &pos );
            GM_SeSet( &work->control.mov, SE_V_ENE_SNORE );

            if ( work->sn_dis < 1000 &&
                 ( GM_PlayerStatus & PLAYER_WATCH ) &&
                 GV_RandU( 12 ) > 10 )
            {
                work->pad.sound = 240;
            }
        }

        if ( work->pad.press & 0x1000 )
        {
            SetMode( work, s14e_800C7F00 );
            UnsetMode( work );
            return;
        }
    }

    if ( !( work->pad.press & 0x200 ) )
    {
        UnsetMode( work );
    }
}

static void s14e_800C9FC0( Work *work ) 
{
    MATRIX world;

    DG_SetPos( &work->body.objs->objs[ 6 ].world );
    DG_MovePos( &BreathPos );
    ReadRotMatrix( &world );
    AN_Unknown_800C3B7C( &world );
}

void s14e_800CA000( Work *work, int time )
{
    int cont;

    work->vision.length = 3000;
    work->act_status |= 0x80;

    if ( time == 0 ) SetAction2( work, 25, 4, 0x3FE );

    if ( time >= 1000 && time <= 1209 )
    {
        cont = time - 1000;
        if ( cont == 0  || cont == 4  || cont == 8  || cont == 12 ||
             cont == 20 || cont == 28 || cont == 36 || cont == 48 ||
             cont == 60 )
        {
            GM_SeSet( &work->control.mov, SE_WET_STEP );
            s14e_800C9FC0( work );
        }
    }
    else if ( time > 30 && ( time & 0x1 ) )
    {
        GM_SeSet( &work->control.mov, SE_WET_STEP );
        if ( time > 45 ) s14e_800C9FC0( work );
    }

    if ( !( work->pad.press & 0x800000 ) ) UnsetMode( work );
}

void s14e_800CA148( Work *work, int time )
{
    work->vision.length = 3000;

    if ( time == 0 ) SetAction2( work, 25, 4, 0x3FE );
    if ( time == 186 ) SetAction2( work, 26, 4, 0xFFFF );

    if ( time == 0   || time == 4   || time == 8   || time == 12  ||
         time == 20  || time == 28  || time == 36  || time == 48  ||
         time == 60  || time == 120 || time == 122 || time == 124 ||
         time == 126 || time == 180 || time == 182 || time == 184 ||
         time == 186 )
    {
        GM_SeSet( &work->control.mov, SE_WET_STEP );
        s14e_800C9FC0( work );
    }

    if ( ( time > 186 && work->body.time2 != 0 ) || !( work->pad.press & 0x1000000 ) )
    {
        work->pad.time = 0;
        UnsetMode( work );
    }
}

void s14e_800CA2E4( Work *work, int unit, int count )
{
    MATRIX world;

    DG_SetPos( &work->body.objs->objs[ unit ].world );
    DG_MovePos( &BloodPos );
    DG_RotatePos( &BloodRot );
    ReadRotMatrix( &world );
    NewBlood( &world, count );
}

void s14e_800CA354( Work *work )
{
    MATRIX world;
    SVECTOR pos;

    DG_SetPos( &work->body.objs->objs[ 1 ].world );
    DG_MovePos( &FogPos );
    DG_RotatePos( &FogRot );
    ReadRotMatrix( &world );
    DG_COPY_VEC( &pos, (VECTOR *)world.t );
    AN_Fog( &pos );
}

void s14e_800CA3B8( Work *work )
{
    SVECTOR step;
    ITEM_INFO info;
    SVECTOR *pos;

    pos = &work->control.mov;

    step = work->act.field_0C;
    step.vx += GV_RandU( 10 );
    step.vy += 100;
    step.vz += GV_RandU( 10 );

    GV_RandU( 4 );

    if ( GM_Weapons[ WP_Famas ] < 0 )
    {
        info.message = ItemText[ 1 ];
        info.type = 2;
        info.id = WP_Socom;
        info.num = 12;
    }
    else
    {
        info.message = ItemText[ 2 ];
        info.type = 2;
        info.id = WP_Famas;
        info.num = 25;
    }

    info.time = 900;
    NewItemPut( pos, &step, &info );
}
void s14e_800CA49C( Work *work, int mark )
{
    MATRIX *world;

    if ( !( work->control.map->index & GM_PlayerMap ) ) return;
    world = &work->body.objs->objs[ 6 ].world;

    if ( mark == 0 ) GM_SeSet( &work->control.mov, SE_EXCLAMATION );
    if ( work->mark_time != 0 ) GV_DestroyActor( work->mark );

    work->mark = AN_HeadMark( world, mark );
    work->mark_time = 30;
}

static void s14e_800CA538( Work *work )
{
    int action, frame, l, r;

    action = work->act.motion1;
    frame = work->m_ctrl.info1.frame;
    l = 160 + ( work->param.index % 4 ) * 2;
    r = 160 + ( work->param.index % 4 ) * 2 + 1;

    if ( action == 1 )
    {
        if ( frame == 22 )
        {
            GM_SeSet( &work->control.mov, r );
        }
        else if ( frame == 11 )
        {
            GM_SeSet( &work->control.mov, l );
        }
    }
    else if ( action == 2 )
    {
        if ( frame == 16 )
        {
            GM_SeSet( &work->control.mov, r );
        }
        else if ( frame == 8 )
        {
            GM_SeSet( &work->control.mov, l );
        }
    }
}
static void s14e_800CA5D0( Work *work )
{
    int frame;

    if ( ZAKO14Command.alert_mode == 1 ) return;
    if ( !( work->control.map->index & GM_PlayerMap ) ) return;

    if ( work->act.motion2 == 20 )
    {
        if ( work->m_ctrl.info2.frame == 31 )
        {
            AN_Breath( &work->body.objs->objs[ 6 ].world );
        }
    }
    else if ( work->act.motion2 == 22 )
    {
        frame = work->m_ctrl.info2.frame;
        if ( frame == 15 || frame == 35 || frame == 50 || frame == 60 ||
             frame == 70 || frame == 74 || frame == 78 )
        {
            AN_Breath( &work->body.objs->objs[ 6 ].world );
        }
    }
    else if ( work->act.motion2 == 19 )
    {
        frame = work->m_ctrl.info2.frame;
        if ( frame == 30  || frame == 40 || frame == 50 || frame == 60 ||
             frame == 70  || frame == 80 || frame == 90 || frame == 95 ||
             frame == 100 || frame == 105 )
        {
            AN_Breath( &work->body.objs->objs[ 6 ].world );
        }
    }
    else
    {
        if ( ( GV_Time % 64 ) == ( work->param.index * 16 ) )
        {
            AN_Breath( &work->body.objs->objs[ 6 ].world );
        }
    }
}


static void s14e_800CA718( Work *work )
{
    if ( work->vision.pad != 2 ) return;
    NewLSight( &GM_PlayerPosition, &work->control.mov, MAKE_RGB( 143, 0, 0 ) );
}

static void s14e_800CA754( Work *work )
{
    SVECTOR rot;
    MATRIX bullet_pos;
    MATRIX *world;
    int flag;

    rot = DG_ZeroVector;
    rot.vz = GV_RandU( 128 );

    world = &work->body.objs->objs[ 4 ].world;
    DG_SetPos( world );
    DG_MovePos( &BulletPos );
    DG_RotatePos( &rot );
    rot.vx = GV_RandS( 16 ) + 1024;
    rot.vz = 0;
    DG_RotatePos( &rot );
    ReadRotMatrix( &bullet_pos );

    if ( GV_Time % 4 )
    {
        flag = BULLET_RECOILSPARK;
        NewBulletEx( flag, &bullet_pos, ENEMY_SIDE, 1, 0, 10, work->param.damage, 10000, 750 );
    }
    else
    {
        flag = BULLET_RECOILSPARK | BULLET_NO_HZD;
        NewBulletEx( flag, &bullet_pos, ENEMY_SIDE, 1, 0, 10, work->param.damage, 10000, 750 );
    }

    GM_SeSetMode( &work->control.mov, SE_ENEMY_SHOT, GM_SEMODE_BOMB );
    AN_BulletSmoke( world, 0 );
    AN_FamasFlash( &bullet_pos );

    s14e_800CA8D8( work, s14e_800CA754 );
}

int s14e_800CA890( Work *work, int index )
{
    int i;

    for ( i = 0; i < 8; i++ )
    {
        if ( work->putchar[ i ] == NULL )
        {
            work->putchar[ i ] = PutChars[ index ];
            return 1;
        }
    }

    return 0;
}

int s14e_800CA8D8( Work *work, void *func )
{
    int i;
    
    for ( i = 0; i < 8; i++ )
    {
        if ( work->putchar[ i ] == func )
        {
            work->putchar[ i ] = NULL;
            return 1;
        }
    }

    return 0;
}

void s14e_800CA90C( Work *work )
{
    int i;
    void ( *func )( Work * );

    for ( i = 0; i < 8; i++ )
    {
        func = work->putchar[ i ];
        if ( func != NULL ) func( work );
    }
}

void s14e_800CA96C( Work *work )
{
    VISION *vision;
    RADAR_SIGHT_PARAM *r_param;

    vision = &work->vision;
    r_param = &work->control.radar_param;

    r_param->dir = vision->facedir;
    r_param->dis = vision->length;
    r_param->range = vision->range * 2;
    r_param->r = 0;
}

void s14e_800CA998( Work *work )
{
    if ( work->modetime[ 7 ] & 0x4 )
    {
        if ( work->target->damaged & ( TARGET_CAPTURE | TARGET_POWER ) )
        {
            work->modetime[ 6 ] |= 0x4;
        }
        else if ( ( work->touch.damaged & TARGET_TOUCH ) &&
                  ( GM_PlayerStatus & ( PLAYER_MOVE | PLAYER_CB_BOX ) ) != PLAYER_CB_BOX )
        {
            work->modetime[ 6 ] |= 0xC;
        }
    }
}

int s14e_800CAA1C( HZD_HDL *hzd, SVECTOR *vec1, SVECTOR *vec2 )
{
    int from, to;

    from = HZD_GetAddress( hzd, vec1, -1 );
    to = HZD_GetAddress( hzd, vec2, -1 );
    from &= 0xFF;
    to &= 0xFF;
    return HZD_ZoneDistance( hzd, from, to );
}

void s14e_800CAA7C( Work *work )
{
    CONTROL *control;

    if ( !( work->modetime[ 7 ] & 0x1 ) ) return;
    if ( work->act_status & 0x68 ) return;

    control = &work->control;
    if ( !( control->map->index & GM_PlayerMap ) ) return;
    if ( GM_NoisePower == 0 ) return;

    switch ( GM_NoisePower )
    {
    case 5:
        if ( GV_DiffVec3( &GM_NoisePosition, &control->mov ) < 500 ) work->modetime[ 6 ] |= 0x1;
        break;
    case 200:
        if ( GV_DiffVec3( &GM_NoisePosition, &control->mov ) < 8000 ) work->modetime[ 6 ] |= 0x1;
        break;
    case 100:
        if ( GV_DiffVec3( &GM_NoisePosition, &control->mov ) >= 8000 ) break;
        if ( s14e_800CAA1C( control->map->hzd, &control->mov, &GM_NoisePosition ) >= 300 ) break;
        work->modetime[ 6 ] |= 0x1;
        GM_NoiseLength = 0;
        GM_NoisePower = 0;
        break;
    case 255:
        work->modetime[ 6 ] |= 0x1;
        break;
    }
}

void s14e_800CABFC( Work *work )
{
    if ( !( work->modetime[ 7 ] & 0x2 ) ) return;
    if ( work->vision.pad != 2 ) return;
    if ( !( GM_PlayerStatus & PLAYER_CB_BOX ) ) return;

    if ( GV_DiffVec3( &work->player_pos, &GM_PlayerPosition ) > 50 ||
         work->player_dir != GM_WhereList[ 0 ]->rot.vy )
    {
        if ( ZAKO14Command.alert_mode == 1 ) return;
        work->player_pos = GM_PlayerPosition;
        work->player_dir = GM_WhereList[ 0 ]->rot.vy;
        work->modetime[ 6 ] |= 0x2;
    }
    else if ( GV_DiffVec3( &work->control.mov, &GM_PlayerPosition ) < 1500 )
    {
        work->modetime[ 6 ] |= 0x40;
    }

    work->vision.pad = 0;
    work->modetime[ 6 ] |= 0x80;
}

void s14e_800CAD24( Work *work )
{
    HZD_HDL *hzd;
    SVECTOR *pos;
    VISION *vis;

    if ( !( work->modetime[ 7 ] & 0x10 ) || work->think2 == 5 ) return;

    hzd = work->control.map->hzd;
    pos = &work->control.mov;
    vis = &work->vision;

    if ( AsiatoCheck( hzd, pos ) &&
         SearchNearAsiato( hzd, pos, vis->facedir, vis->range, vis->length ) >= 0 )
    {
        work->modetime[ 6 ] |= 0x10;
    }
}

void s14e_800CADD0( Work *work )
{
    work->modetime[ 6 ] = 0;
    s14e_800CAA7C( work );

    if ( ZAKO14Command.alert_mode != 1 )
    {
        s14e_800CA998( work );
        s14e_800CABFC( work );
        s14e_800CAD24( work );
    }
}

void s14e_800CAE24( Work *work )
{
    switch ( work->vision.pad )
    {
    case 0:
        if ( work->field_B9C != 2 ||
             !( GM_PlayerStatus & PLAYER_INTRUDE ) ||
             work->vision.length == 0 )
        {
            work->field_B9C = 0;
            work->alert_level -= 4;
        }
        break;
    case 1:
        work->field_B9C = 1;
        work->alert_level--;
        break;
    case 2:
        work->field_B9C = 2;
        work->alert_level++;
        break;
    }

    if ( work->alert_level < 0 )
    {
        work->alert_level = 0;
    }
    else if ( work->alert_level  > 255 )
    {
        work->alert_level = 255;
    }
}

void s14e_800CAEF4( Work *work )
{
    SVECTOR tmp;
    CONTROL *control;
    SVECTOR *pos;
    int dir, dis, height;
    MAP *map;

    control = &work->control;
    pos = &GM_PlayerPosition;
    GV_SubVec3( pos, &control->mov, &tmp );

    dir = GV_VecDir2( &tmp );
    work->sn_dir = dir;

    dis = GV_VecLen3( &tmp );
    work->sn_dis = dis;

    height = ABS( pos->vy - work->control.mov.vy );

    if ( !( work->control.map->index & GM_PlayerMap ) )
    {
        work->vision.pad = 0;
        return;
    }

    if ( GM_PlayerStatus & PLAYER_INTRUDE )
    {
        work->vision.pad = 0;
        return;
    }

    if ( ( ZAKO14Command.time % ZAKO14Command.n_watchers ) != work->param.index )
    {
        return;
    }

    if ( dis > ( ZAKO14_EYE_LENGTH + 2000 ) )
    {
        work->vision.pad = 0;
        return;
    }

    if ( height > 2000 )
    {
        work->vision.pad = 0;
        return;
    }

    if ( dis >= 500 && GV_DiffDirAbs( work->vision.facedir, dir ) >= work->vision.range )
    {
        work->vision.pad = 0;
        return;
    }

    map = work->control.map;
    if ( !HZD_OnlineHazardCheck( map->hzd, pos, &control->mov, HZD_CHK_ALL, HZD_SEG_NO_PLAYER ) &&
         !GM_OnlineTargetCheckAny( &control->mov, pos, map->index, &tmp ) )
    {
        if ( dis > work->vision.length )
        {
            work->vision.pad = 1;
        }
        else
        {
            work->vision.pad = 2;
        }
    }
    else
    {
        work->vision.pad = 0;
    }
}

void s14e_800CB0BC( Work *work )
{
    if ( work->act.motion1 <= 48 )
    {
        s14e_800CA96C( work );
        s14e_800CAEF4( work );
        s14e_800CADD0( work );
        s14e_800CAE24( work );
        s14e_800CDC08( work );
        s14e_800CA90C( work );
    }

    s14e_800C96AC( work );
}

void s14e_800CB128( Work *work )
{
    TARGET *trg;
    int turn, motion1;
    CONTROL *control;

    trg = work->target;
    turn = 0;

    if ( !( trg->damaged & TARGET_PUSH ) ) return;

    GV_AddVec3( &trg->offset, &work->control.step, &work->control.step );
    trg->damaged &= ~TARGET_PUSH;

    motion1 = work->act.motion1;
    if ( motion1 == 1 || motion1 == 2 )
    {
        if ( trg->offset.pad != 0 )
        {
            if ( GV_Time & 256 )
            {
                if ( work->param.index & 1 )
                {
                    turn = trg->offset.pad * 1024;
                }
                else
                {
                    turn = ( trg->offset.pad + 2 ) * 1024;
                }
            }
            else
            {
                if ( work->param.index & 1 )
                {
                    turn = ( trg->offset.pad + 2 ) * 1024;
                }
                else
                {
                    turn = trg->offset.pad * 1024;
                }
            }

            turn &= 4095;
        }

        control = &work->control;
        control->turn.vy = turn;
        control->r_sphere = GV_NearExp2( control->r_sphere, work->act.field_1C );
    }
}

