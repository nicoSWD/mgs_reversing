#include "animal/zako14/zako.h"

#include <stdio.h>
#include "enemy/eyeflash.h"

extern SVECTOR   ZAKO14_PlayerPosition;
extern COMMANDER ZAKO14Command;
extern TOP       ZAKO14TOPCOMMAND;
extern int       ZAKO14_Unknown;
extern int       ZAKO14_GameFlag;
extern int       ZAKO14_PlayerAddress;
extern int       ZAKO14_PlayerMap;

void s14e_800CA49C( Work *work, int mark );
void s14e_800CDB78( Work *work );
int  s14e_800CCCC4( Work *work );

static SVECTOR s14e_dword_800C34D8[] = {
    { 5750, 0, -1000 },
    { 6250, 0, 500 },
    { 5750, 0, 2000 }
};

int s14e_dword_800C34F0 = 0x00000000;
int s14e_dword_800C34F4 = 0x00000000;
int s14e_dword_800C34F8 = 0x00000001;
int s14e_dword_800C34FC = 0x00000000;
int s14e_dword_800C3500 = 0x00000000;
int s14e_dword_800C3504 = 0x00000001;
int s14e_dword_800C3508 = 0x00000000;
int s14e_dword_800C350C = 0x00000000;
int s14e_dword_800C3510 = 0x00000001;
int s14e_dword_800C3514 = 0x00000000;
int s14e_dword_800C3518 = 0x00000000;
int s14e_dword_800C351C = 0x00000001;
int s14e_dword_800C3520 = 0x00000000;
int s14e_dword_800C3524 = 0x00000000;
int s14e_dword_800C3528 = 0x00000000;
int s14e_dword_800C352C = 0x00000000;
int s14e_dword_800C3530 = 0x00000000;
int s14e_dword_800C3534 = 0x00000000;
int s14e_dword_800C3538 = 0x00000001;
int s14e_dword_800C353C = 0x00000000;
int s14e_dword_800C3540 = 0x00000001;
int s14e_dword_800C3544 = 0x00000002;
int s14e_dword_800C3548 = 0x00000000;
int s14e_dword_800C354C = 0x00000001;
int s14e_dword_800C3550 = 0x00000002;
int s14e_dword_800C3554 = 0x00000000;
int s14e_dword_800C3558 = 0x00000000;
int s14e_dword_800C355C = 0x00000001;
int s14e_dword_800C3560 = 0x00000000;
int s14e_dword_800C3564 = 0x00000000;
int s14e_dword_800C3568 = 0x00000000;
int s14e_dword_800C356C = 0x00000000;
int s14e_dword_800C3570 = 0x00000000;
int s14e_dword_800C3574 = 0x00000001;
int s14e_dword_800C3578 = 0x00000002;
int s14e_dword_800C357C = 0x00000000;
int s14e_dword_800C3580 = 0x00000000;
int s14e_dword_800C3584 = 0x00000001;
int s14e_dword_800C3588 = 0x00000000;
int s14e_dword_800C358C = 0x00000000;
int s14e_dword_800C3590 = 0x00000001;
int s14e_dword_800C3594 = 0x00000000;
int s14e_dword_800C3598 = 0x00000001;
int s14e_dword_800C359C = 0x00000002;
int s14e_dword_800C35A0 = 0x00000000;
int s14e_dword_800C35A4 = 0x00000000;
int s14e_dword_800C35A8 = 0x00000000;
int s14e_dword_800C35AC = 0x00000000;
int s14e_dword_800C35B0 = 0x00000000;
int s14e_dword_800C35B4 = 0x00000000;
int s14e_dword_800C35B8 = 0x00000000;
int s14e_dword_800C35BC = 0x00000000;
int s14e_dword_800C35C0 = 0x00000000;
int s14e_dword_800C35C4 = 0x00000000;
int s14e_dword_800C35C8 = 0x00000000;
int s14e_dword_800C35CC = 0x00000000;
int s14e_dword_800C35D0 = 0x00000000;
int s14e_dword_800C35D4 = 0x00000000;
int s14e_dword_800C35D8 = 0x00000000;
int s14e_dword_800C35DC = 0x00000000;
int s14e_dword_800C35E0 = 0x00000000;
int s14e_dword_800C35E4 = 0x00000000;
int s14e_dword_800C35E8 = 0x00000000;
int s14e_dword_800C35EC = 0x00000000;
int s14e_dword_800C35F0 = 0x00000000;
int s14e_dword_800C35F4 = 0x00000000;
int s14e_dword_800C35F8 = 0x00000003;
int s14e_dword_800C35FC = 0x00000003;
int s14e_dword_800C3600 = 0x00000003;
int s14e_dword_800C3604 = 0x00000002;
int s14e_dword_800C3608 = 0x00000002;
int s14e_dword_800C360C = 0x00000002;
int s14e_dword_800C3610 = 0x00000003;
int s14e_dword_800C3614 = 0x00000003;
int s14e_dword_800C3618 = 0x00000003;
int s14e_dword_800C361C = 0x00000000;
int s14e_dword_800C3620 = 0x00000000;
int s14e_dword_800C3624 = 0x00000000;
int s14e_dword_800C3628 = 0x00000000;
int s14e_dword_800C362C = 0x00000000;
int s14e_dword_800C3630 = 0x00000001;
int s14e_dword_800C3634 = 0x00000001;
int s14e_dword_800C3638 = 0x00000002;
int s14e_dword_800C363C = 0x00000002;
int s14e_dword_800C3640 = 0x00000020;
int s14e_dword_800C3644 = 0x00000020;
int s14e_dword_800C3648 = 0x00000003;
int s14e_dword_800C364C = 0x00000009;
int s14e_dword_800C3650 = 0x00000009;
int s14e_dword_800C3654 = 0x00000002;
int s14e_dword_800C3658 = 0x00000002;
int s14e_dword_800C365C = 0x00000001;
int s14e_dword_800C3660 = 0x00000000;
int s14e_dword_800C3664 = 0x00000000;
int s14e_dword_800C3668 = 0x00000000;
int s14e_dword_800C366C = 0x00000000;
int s14e_dword_800C3670 = 0x00000003;
int s14e_dword_800C3674 = 0x00000017;
int s14e_dword_800C3678 = 0x00000017;
int s14e_dword_800C367C = 0x00000000;
int s14e_dword_800C3680 = 0x00000000;
int s14e_dword_800C3684 = 0x00000001;
int s14e_dword_800C3688 = 0x00000001;
int s14e_dword_800C368C = 0x00000001;
int s14e_dword_800C3690 = 0x00000000;
int s14e_dword_800C3694 = 0x00000000;
int s14e_dword_800C3698 = 0x00000006;
int s14e_dword_800C369C = 0x00000006;
int s14e_dword_800C36A0 = 0x00000000;
int s14e_dword_800C36A4 = 0x00000000;
int s14e_dword_800C36A8 = 0x00000000;
int s14e_dword_800C36AC = 0x00000000;
int s14e_dword_800C36B0 = 0x00000002;
int s14e_dword_800C36B4 = 0x00000002;
int s14e_dword_800C36B8 = 0x00000002;
int s14e_dword_800C36BC = 0x00000002;
int s14e_dword_800C36C0 = 0x00000000;
int s14e_dword_800C36C4 = 0x00000000;
int s14e_dword_800C36C8 = 0x00000000;
int s14e_dword_800C36CC = 0x00000000;
int s14e_dword_800C36D0 = 0x00000000;
int s14e_dword_800C36D4 = 0x00000000;
int s14e_dword_800C36D8 = 0x00000000;
int s14e_dword_800C36DC = 0x00000000;
int s14e_dword_800C36E0 = 0x00000000;
int s14e_dword_800C36E4 = 0x00000000;
int s14e_dword_800C36E8 = 0x00000000;
int s14e_dword_800C36EC = 0x00000000;


void s14e_800CCB60( Work *work )
{
    work->target_addr = ZAKO14_PlayerAddress;
    work->target_pos = ZAKO14_PlayerPosition;
    work->target_map = ZAKO14_PlayerMap;
}

void s14e_800CCBA8( Work *work )
{
    HZD_ZON *zone;

    zone = HZD_GetZone( work->control.map->hzd, work->param.next );
    work->target_addr = work->param.next | ( work->param.next << 8 );
    work->target_pos.vx = zone->x;
    work->target_pos.vy = zone->y;
    work->target_pos.vz = zone->z;
    work->target_map = work->start_map;
}

void s14e_800CCC04( Work *work )
{
    if ( work->param.next == 255 )
    {
        work->think2 = 1;
        work->think1 = 0;
        work->think3 = 4;
    }
    else
    {
        work->think2 = 3;
        work->think1 = 0;
        work->think3 = 16;
        s14e_800CCBA8( work );
    }

    work->count3 = 0;
    work->pad.field_4 = 1;
}

void s14e_800CCC70( Work *work )
{
    work->think1 = 0;
    work->think2 = 0;
    work->think3 = 0;
    work->count3 = 0;
    work->pad.field_4 = 1;
}

int s14e_800CCC8C( SVECTOR *from, SVECTOR *to )
{
    SVECTOR diff;

    GV_SubVec3( to, from, &diff );
    diff.vy = 0;

    return GV_VecDir2( &diff );
}

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CCCC4.s")
int s14e_800CCF84( Work *work )
{
    if ( work->count3 == 0 )
    {
        s14e_800CA49C( work, 0 );
        GM_SeSet( &work->control.mov, SE_EXCLAMATION );
        NewEyeflash( &work->body.objs->objs[ 6 ].world, &work->control.mov, "kirari01", 0 );
    }

    if ( work->count3 >= 0 && work->count3 <= 19 )
    {
        work->pad.press |= 0x20;
        work->count3++;
        return 0;
    }

    return 1;
}

int s14e_800CD024( Work *work )
{
    if ( work->count3 == 0 )
    {
        GM_SeSet( &work->control.mov, 83 );
        s14e_800CA49C( work, 0 );
    }

    if ( work->count3 < 0 || work->count3 > 20 ) return 1;

    work->count3++;
    return 0;
}

int s14e_800CD08C( Work *work )
{
    if ( !( work->count3 & 0x1F ) )
    {
        work->current_addr = -1;
        work->last_addr = -1;
        s14e_800CCCC4( work );
    }

    work->pad.dir = s14e_800CCCC4( work );
    if ( work->pad.dir < 0 ) return 1;

    work->count3++;
    return 0;
}
int s14e_800CD0FC( Work *work, int r )
{
    SVECTOR diff;
    int x, z;

    diff.vx = x = work->target_pos.vx - work->control.mov.vx;
    diff.vz = z = work->target_pos.vz - work->control.mov.vz;

    if ( x <= -r || x >= r || z <= -r || z >= r )
    {
        work->pad.dir = GV_VecDir2( &diff );
        work->count3++;
        return 0;
    }

    return 1;

}

int s14e_800CD184( Work *work )
{
    s14e_800CCB60( work );
    if ( work->control.map->index != work->target_map ) return -1;

    if ( work->count3 & 0x10 )
    {
        work->current_addr = HZD_GetAddress( work->control.map->hzd, &work->control.mov, -1 );
        if ( HZD_ReachTo( work->control.map->hzd, work->current_addr, work->target_addr ) >= 2 ) return -1;
    }

    return s14e_800CD0FC( work, 1500 );
}

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CD218.s")
int s14e_800CD2B4( Work *work )
{
    work->pad.press |= 0x20000;
    if ( work->count3 == 0 ) work->pad.press |= 0x80000;
    if ( work->count3 < 0 || work->count3 > 80 ) return 1;

    switch ( work->param.next )
    {
    case 0:
        work->pad.dir = 1536;
        break;
    case 9:
        work->pad.dir = 2048;
        break;
    case 3:
        work->pad.dir = 2560;
        break;
    case 23:
        work->pad.dir = 3072;
        break;
    case 2:
        work->pad.dir = 3584;
        break;
    case 32:
        work->pad.dir = 0;
        break;
    case 1:
        work->pad.dir = 512;
        break;
    case 6:
        work->pad.dir = 1024;
        break;
    }

    work->count3++;
    return 0;
}

int s14e_800CD37C( Work *work )
{
    work->pad.press |= 0x10000;

    if ( work->count3 == 30 )
    {
        work->pad.press |= 0x100000;
        work->field_C04 = s14e_dword_800C34D8[ work->param.index ];
    }
    else if ( work->body.is_end )
    {
        return 1;
    }

    work->pad.dir = s14e_800CCC8C( &work->control.mov, &s14e_dword_800C34D8[ work->param.index ] );
    work->count3++;
    return 0;
}

int s14e_800CD440( Work *work )
{
    work->pad.press |= 0x10000;

    if ( work->count3 == 0 )
    {
        work->pad.press |= 0x200000;
    }
    else if ( work->field_8FC != 0 || ( work->count3 < 0 || work->count3 > 30 ) )
    {
        return 1;
    }

    work->pad.dir = work->sn_dir;
    work->count3++;
    return 0;
}

int s14e_800CD4A8( Work *work )
{
    int count;

    count = work->count3;
    work->pad.press |= 0x10000;
    work->pad.dir = work->sn_dir;

    if ( count < 10 )
    {
        if ( !( count & 1 ) ) work->pad.press |= 0x40000;
    }
    else if ( count > 19 )
    {
        return 1;
    }

    work->count3++;
    return 0;
}
int s14e_800CD514( Work *work )
{
    if ( work->count3 > 200 ) return 1;

    work->pad.press |= 0x10000;
    work->pad.dir = work->sn_dir;
    work->count3++;
    return 0;
}
int s14e_800CD55C( Work *work )
{
    if ( work->sn_dis < 800 )
    {
        work->pad.press |= 0x10000;
        return 1;
    }

    work->pad.dir = work->sn_dir;
    work->count3++;
    return 0;
}

const char s14e_dword_800DCFC4[] = {0x0, 0x0, 0x0, 0x0};
const int s14e_dword_800DCFC8 = 0x800CD654;
const int s14e_dword_800DCFCC = 0x800CD69C;
const int s14e_dword_800DCFD0 = 0x800CD684;
const int s14e_dword_800DCFD4 = 0x800CD68C;
const int s14e_dword_800DCFD8 = 0x800CD694;
const char s14e_dword_800DCFDC[] = {0x0, 0x0, 0x0, 0x0};
const int s14e_dword_800DCFE0 = 0x800CD8A4;
const int s14e_dword_800DCFE4 = 0x800CD8BC;
const int s14e_dword_800DCFE8 = 0x800CD9AC;
const int s14e_dword_800DCFEC = 0x800CD8D4;
const int s14e_dword_800DCFF0 = 0x800CD8F4;
const int s14e_dword_800DCFF4 = 0x800CD90C;
const int s14e_dword_800DCFF8 = 0x800CD924;
const int s14e_dword_800DCFFC = 0x800CD93C;
const int s14e_dword_800DD000 = 0x800CD970;
const int s14e_dword_800DD004 = 0x800CD954;
const int s14e_dword_800DD008 = 0x800CDBB0;
const int s14e_dword_800DD00C = 0x800CDBC0;
const int s14e_dword_800DD010 = 0x800CDBD0;
const int s14e_dword_800DD014 = 0x800CDBE0;
const int s14e_dword_800DD018 = 0x800CDBF0;

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CD5A4.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CD6C4.s")
void s14e_800CD714( Work *work )
{
    s14e_800CCC04( work );
}

void s14e_800CD734( Work *work )
{
    if ( work->think3 == 0 )
    {
        s14e_800CD714( work );
    }
}

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CD764.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CD858.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CD9DC.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CDADC.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800CDB78.s")
void s14e_800CDC08( Work *work )
{
    work->pad.dir = -1;
    work->control.radar_atr = RADAR_VISIBLE | RADAR_SIGHT;
    work->pad.press = 0;
    work->alert_level = 255;

    if ( work->think1 == 0 )
    {
        work->act.field_1E = 1;
        s14e_800CDB78( work );
    }
}
