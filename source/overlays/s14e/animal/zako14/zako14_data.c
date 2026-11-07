#include "animal/zako14/zako.h"

#include "enemy/grnad_e.h"
#include "game/vibrate.h"

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

void s14e_800C94A8( Work *work, int time );
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
int s14e_dword_800C33CC = 0xFD440000;
int s14e_dword_800C33D0 = 0x00000384;
int s14e_dword_800C33D4 = 0x00000000;
int s14e_dword_800C33D8 = 0x00000064;
int s14e_dword_800C33DC = 0x0000FC00;
int s14e_dword_800C33E0 = 0x00000000;
int s14e_dword_800C33E4 = 0x00000000;
int s14e_dword_800C33E8 = 0x00000064;
int s14e_dword_800C33EC = 0x0000FC00;
int s14e_dword_800C33F0 = 0x00000000;
int s14e_dword_800C33F4 = 0x800DCA58;
int s14e_dword_800C33F8 = 0x800DCA44;
int s14e_dword_800C33FC = 0x800DCA30;
int s14e_dword_800C3400 = 0x800CA538;
int s14e_dword_800C3404 = 0x800CA5D0;
int s14e_dword_800C3408 = 0x800CA718;
int s14e_dword_800C340C = 0x800CA754;
int s14e_dword_800C3410 = 0xFE0C0005;
int s14e_dword_800C3414 = 0x00000050;


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


#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C6D20.s")
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
const char s14e_dword_800DCA30[] = "FA-MAS/BULLET * 25";
const char s14e_dword_800DCA44[] = "SOCOM/BULLET * 12";
const char s14e_dword_800DCA58[] = "RATION";

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

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7398.s")
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

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7978.s")
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

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7B9C.s")
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

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C8AD8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C8BE8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C8CF8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C924C.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C94A8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C95B8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C96AC.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C9838.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C9904.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C99D0.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C9A68.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C9B00.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C9BB4.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C9C7C.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C9D20.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C9DE8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C9FC0.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA000.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA148.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA2E4.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA354.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA3B8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA49C.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA538.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA5D0.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA718.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA754.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA890.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA8D8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA90C.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA96C.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CA998.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CAA1C.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CAA7C.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CABFC.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CAD24.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CADD0.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CAE24.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CAEF4.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CB0BC.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CB128.s")
