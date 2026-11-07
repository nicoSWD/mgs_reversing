#include "animal/zako14/zako.h"

#include "enemy/grnad_e.h"
#include "game/vibrate.h"

int  s14e_800C6E08( Work *work );
void s14e_800C7398( Work *work, int time );
void s14e_800C7978( Work *work, int time );
void s14e_800C7FA8( Work *work, int time );
void s14e_800C8350( Work *work, int time );
void s14e_800C8898( Work *work, int time );
void s14e_800C8A10( Work *work, int time );
void s14e_800C8BE8( Work *work, int time );
void s14e_800C8CF8( Work *work, int time );
void s14e_800C86D4( Work *work, int time );
void s14e_800C8748( Work *work, int time );
void s14e_800C8AD8( Work *work, int time );

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
int s14e_dword_800C33C4 = 0x0000FF6A;
int s14e_dword_800C33C8 = 0x0000012C;
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

const char s14e_dword_800DC9CC[] = {'8', 0xff, 0xe8, 0x3};
const char s14e_dword_800DC9D0[] = {'X', 0x2, 0x0, 0x0};
const char s14e_dword_800DC9D4[] = {0x5, 0x0, 0x0, 0x0};
const char s14e_dword_800DC9D8[] = "d";
const char s14e_dword_800DC9DC[] = {0x90, 0x1, 0xe8, 0x3};
const char s14e_dword_800DC9E0[] = {0x90, 0x1, 0x0, 0x0};
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

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C6D20.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C6DB0.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C6E08.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C6E6C.s")
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

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C701C.s")
void s14e_800C712C( Work *work )
{
    TARGET *target;

    target = &work->attack;
    GM_SetPowerTarget( target, POWER_CONST | POWER_EXPLODE, 5, 0, 3, &ZAKO14_ATTACK_FORCE );
    GM_MoveTarget( target, &work->control.mov );
    GM_PowerTarget( target );
}

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7194.s")
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


void s14e_800C7970( void )
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
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7CD0.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7E28.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7F00.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C7FA8.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C82D0.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C8350.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C86D4.s")
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

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C8A10.s")
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
