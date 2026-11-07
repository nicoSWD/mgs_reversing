#define __ZAKO_BUGFIX__

#include "animal/zako14/zako.h"

#include <stdio.h>
#include "mts/mts.h" // for fprintf
#include "libgcl/libgcl.h"
#include "strcode.h"
#include "chara/snake/shadow.h"
#include "enemy/glight.h"

/*----------------------------------------------------------------------------*/

static int ChaseDis[] = {
    3000, 4000, 7000, 8000,
    8000, 8000, 8000, 8000,
};

int s14e_dword_800C3330 = 0x000F012C;
int s14e_dword_800C3334 = 0x003C001E;

int s14e_dword_800C3338 = 0x0000005A;
int s14e_dword_800C333C = 0x75307D01;

const char s14e_dword_800DC908[] = "Err Err Err  Set time Over\n";
const char s14e_dword_800DC924[] = "Err Err Err  Set Dir Over\n";

/*----------------------------------------------------------------------------*/

extern COMMANDER ZAKO14Command;
extern int       ZAKO14_GameFlag;

// zk11aact.c
void s14e_800CB0BC( Work *work ); // Zako14ActionMain
void s14e_800CB128( Work *work ); // Zako14PushMove

// BUG: Should be void GM_ConfigControlRadarparam( CONTROL *, u_short, u_short, u_short, u_short );
extern void GM_ConfigControlRadarparam( CONTROL *ctrl, u_short dir, u_short dis, int range, u_short r );

/*----------------------------------------------------------------------------*/

void s14e_800C5958( Work *work )
{
    /* do nothing */
}

void s14e_800C5960( DG_OBJS *objs, DG_DEF *def )
{
    DG_OBJ *obj;
    int i;

    DG_FreeObjsPacket( objs, 0 );
    DG_FreeObjsPacket( objs, 1 );

    objs->def = def;
    obj = objs->objs;
    for ( i = 0; i < 16; i++ )
    {
        obj->model = &def->models[ i ];
        obj->n_packs = def->models[ i ].n_faces;
        obj++;
    }
}

void s14e_800C59DC( Work *work )
{
    if ( work->field_9E4 != 0 )
    {
        if ( work->param.low_poly == 1 )
        {
            if ( ( GM_GameStatus & STATE_CAM_CHANGE ) || GM_Camera.first_person )
            {
                if ( work->lod != 1 )
                {
                    work->lod = 1;
                    s14e_800C5960( work->body.objs, work->lod_models[ 1 ] );
                }
            }
            else
            {
                if ( work->lod != 0 )
                {
                    work->lod = 0;
                    s14e_800C5960( work->body.objs, work->lod_models[ 0 ] );
                }
            }
        }

        DG_VisibleObjs( work->body.objs );
        DG_VisibleObjs( work->weapon.objs );
        *work->shadow_enable = 1;
        *work->glight_enable = 1;
    }
    else
    {
        DG_InvisibleObjs( work->body.objs );
        DG_InvisibleObjs( work->weapon.objs );
        *work->shadow_enable = 0;
        *work->glight_enable = 0;
    }
}


void s14e_800C5B14( Work *work )
{
    VECTOR scale;
    CONTROL *control;
    TARGET *trg, *touch;

    control = &work->control;
    if ( GM_CheckMessage( work, control->name, HASH_KILL ) )
    {
        GV_DestroyActor( work );
        return;
    }

    s14e_800C5958( work );

    if ( work->field_C34 == 0 )
    {
        s14e_800CB128( work );

        GM_ActControl( control );
        GM_ActObject2( &work->body );
        GM_ActObject2( &work->weapon );
        DG_GetLightMatrix2( &control->mov, work->light );

        s14e_800CB0BC( work );

        trg = work->target;
        GM_MoveTarget( trg, &control->mov );
        GM_PushTarget( trg );

        if ( trg->class & TARGET_TOUCH )
        {
            touch = &work->touch;
            if ( touch->class & TARGET_TOUCH )
            {
                if ( touch->damaged & TARGET_TOUCH ) touch->damaged &= ~TARGET_TOUCH;
                GM_MoveTarget( touch, &control->mov );
                GM_TouchTarget( touch );
            }
        }

        scale.vx = scale.vy = scale.vz = work->scale;
        ScaleMatrix( &work->body.objs->world, &scale );
    }

    s14e_800C59DC( work );
    if ( ZAKO14_GameFlag == 15 && ZAKO14Command.watchers[ work->param.index ].field_4 == 1 )
    {
        GV_DestroyActor( work );
    }
}

void s14e_800C5C78( Work *work )
{
    TARGET *trg, *trg2;
    int life, faint;

    trg = work->target;
    life = work->param.life;
    faint = work->param.faint;
    GM_SetTarget( trg, TARGET_FLAG, ENEMY_SIDE, &ZAKO14_TARGET_SIZE );
    GM_SetPowerTarget( trg, POWER_DECREASE, -1, life, faint, &ZAKO14_TARGET_FORCE );
    GM_SetCaptureTarget( trg, -1, faint, NULL, NULL );
    GM_SetC4Target( trg, &work->body.objs->objs[ 1 ].world );

    trg2 = &work->attack;
    work->param.unknown = work->param.life;
    GM_SetTarget( trg2, TARGET_POWER, PLAYER_SIDE, &ZAKO14_ATTACK_SIZE );
    GM_SetPowerTarget( trg2, POWER_CONST | POWER_EXPLODE, 5, 0, 3, &ZAKO14_ATTACK_FORCE );

    trg2 = &work->touch;
    GM_SetTarget( trg2, TARGET_TOUCH, ENEMY_SIDE, &ZAKO14_TOUCH_SIZE );
    GM_SetPowerTarget( trg2, POWER_CONST | POWER_EXPLODE, 5, 0, 0, &ZAKO14_TOUCH_FORCE );
}

void s14e_800C5DAC( Work *work )
{
    ACT *act;

    act = &work->act;
    GV_ZeroMemory( act, sizeof(ACT) );
    act->field_00 = 0;
    act->field_1C = 450;
    act->field_1E = 1;

    work->action = NULL;
    work->action2 = NULL;
    work->time = 0;
    work->time2 = 0;
}

int s14e_800C5E08( Work *work, int name, int where )
{
    SVECTOR indices;
    CONTROL *control;
    OBJECT *body, *weapon;
    char *opt;
    int i;

    control = &work->control;
    if ( GM_InitControl( control, name, where ) < 0 ) return -1;

    GM_ConfigControlString( control, GCL_GetOption( 'p' ), GCL_GetOption( 'd' ) );
    GM_ConfigControlAttribute( control, RADAR_VISIBLE | RADAR_SIGHT | RADAR_ALL_MAP );
    GM_ConfigControlInterp( control, 4 );
    control->seg_flag = HZD_SEG_NO_NAVIGATE;
    GM_ConfigControlTrapCheck( control );

    body = &work->body;
    weapon = &work->weapon;

    GM_InitObject( body, KMD_IPPANHEI, 0x32D, OAR_ENEMY );
    GM_ConfigObjectJoint( body );
    GM_ConfigMotionControl( body, &work->m_ctrl, OAR_ENEMY, work->m_segs1, work->m_segs2, control, work->rots );
    GM_ConfigObjectLight( body, work->light );

    work->param.low_poly = 0;
    opt = GCL_GetOption( 'y' );
    if ( opt != NULL )
    {
        work->param.low_poly = GCL_StrToInt( opt );
    }

    if ( work->param.low_poly == 1 )
    {
        work->lod_models[ 1 ] = body->objs->def;
        work->lod_models[ 0 ] = GV_GetCache( GV_CacheID( KMD_LOPRYHEI, 'k' ) );
        work->lod = 1;
    }

    work->hom = GM_SetHomingTarget( &body->objs->objs[ 6 ].world, control );

    GM_InitObject( weapon, KMD_FAMAS, 0x6D, 0 );
    GM_ConfigObjectLight( weapon, work->light );
    GM_ConfigObjectRoot( weapon, body, 4 );

    for ( i = 0; i < 0; i++ )
    {
        work->putchar[ i ] = NULL;
    }

    indices.vx = 0;
    indices.vy = 6;
    indices.vz = 12;
    indices.pad = 15;
    work->shadow = NewShadow2( control, body, indices, &work->shadow_enable );
    work->glight = NewGunLight( &body->objs->objs[ 4 ].world, &work->glight_enable );

    s14e_800CA890( work, 0 );
    s14e_800C5DAC( work );
    return 0;
}

void s14e_800C6028( Work *work )
{
    ZAKO14ResetWorkID( work->param.index );
    GM_FreeControl( &work->control );
    GM_FreeObject( &work->body );
    GM_FreeObject( &work->weapon );
    GM_FreeTarget( work->target );
    GV_DestroyActor( work->glight );
    GV_DestroyActor( work->shadow );
    GM_ResetHomingTarget( work->hom );
}


void s14e_800C609C( Work *work )
{
    s14e_800C6028( work );
}

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C60BC.s")
int s14e_800C60BC( Work *work );

#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C6158.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C61B0.s")
#pragma INCLUDE_ASM("asm/overlays/s14e/s14e_800C6208.s")
void s14e_800C6208( Work *work );

void s14e_800C62E0( Work *work, int name, int where )
{
    char *opt;
    int addr, i;

    s14e_800C5E08( work, name, where );

    work->param.index = ZAKO14SetWorkID( work );
    if ( work->param.index < 0 )
    {
        printf( "Err not enough work !!\n" );
    }

    s14e_800C6208( work );

    work->param.root = 0;
    opt = GCL_GetOption( 'r' );
    if ( opt != NULL )
    {
        work->param.root = GCL_StrToInt( opt );
    }

    work->param.life = 768;
    opt = GCL_GetOption( 'l' );
    if ( opt != NULL )
    {
        work->param.life = GCL_StrToInt( opt );
    }

    work->param.faint = 7;
    opt = GCL_GetOption( 'f' );
    if ( opt != NULL )
    {
        work->param.faint = GCL_StrToInt( opt );
    }

    work->param.damage = 64;
    opt = GCL_GetOption( 'z' );
    if ( opt != NULL )
    {
        work->param.damage = GCL_StrToInt( opt );
    }

    work->param.blood = 65;
    opt = GCL_GetOption( 'b' );
    if ( opt != NULL )
    {
        work->param.blood = GCL_StrToInt( opt );
    }

    printf( " low[pory=%d\n", work->param.low_poly );

    work->param.g_flag = 255;
    opt = GCL_GetOption( 'g' );
    if ( opt != NULL )
    {
        work->param.g_flag = GCL_StrToInt( opt );
    }

    if ( work->param.blood == 90 )
    {
        work->param.g_flag = 0;
    }

    work->chase_dis = ChaseDis[ work->param.index ];
    work->chase_index = work->param.index;

    work->param.next = 255;
    opt = GCL_GetOption( 'n' );
    if ( opt != NULL )
    {
        work->param.next = GCL_StrToInt( opt );
    }

    work->param.area = 'A';
    opt = GCL_GetOption( 'a' );
    if ( opt != NULL )
    {
        work->param.area = GCL_StrToInt( opt );
    }

    if ( work->param.area == 'S' )
    {
        s14e_800CA890( work, 1 );
    }

    work->scale = 4096;
    opt = GCL_GetOption( 's' );
    if ( opt != NULL )
    {
        work->scale += GCL_StrToInt( opt );
    }

    work->param.item = 1;

    if ( s14e_800C60BC( work ) < 0 )
    {
        fprintf( 1, "watchar.c : action point Err\n" );
    }

    work->target = GM_AllocTarget();
    if ( work->target != NULL )
    {
        s14e_800C5C78( work );
    }

    work->think1 = 0;
    work->think2 = 0;
    work->think3 = 0;
    work->think4 = 0;
    work->count3 = 0;
    work->l_count = 0;
    work->t_count = 0;
    work->mark_time = 0;
    work->next_node = 0;
    work->search_flag = 0;
    work->act_status = 0;

    work->target_pos = work->nodes[ 0 ];
    work->target_addr = HZD_GetAddress( work->control.map->hzd, &work->target_pos, -1 );
    work->target_map = GM_CurrentMap;
    work->alert_level = 0;

    work->field_9E4 = 1;
    work->vision.range = 512;
    work->vision.facedir = 0;
    work->field_B9C = 0;
    work->pad.sound = 0;
    work->pad.time = 0;
    work->vision.length = ZAKO14_EYE_LENGTH;
    work->player_pos = ZAKO14_NO_POINT;
    work->subweapon = NULL;
    work->control.mov = work->nodes[ 0 ];
    work->field_C44 = 0;
    work->param.c_root = 0;
    work->param.c_next = work->param.next;

    for ( i = 0; i < 8; i++ )
    {
        work->modetime[ i ] = 0;
    }

    work->modetime[ 7 ] = 0x7;
    work->modetime[ 4 ] = 0xFF;

    GM_ConfigControlRadarparam( &work->control, 0, 512, ZAKO14_EYE_LENGTH, 0 );

    work->start_pos = work->nodes[ 0 ];
    work->start_map = GM_CurrentMap;

    addr = HZD_GetAddress( work->control.map->hzd, &work->control.mov, -1 );
    work->field_BE8 = addr;
    work->next_addr = addr;
    work->last_addr = addr;
    work->next_pos = work->start_pos;
}

void *NewZako14( int name, int where )
{
    Work *work;

    work = GV_NewActor( GV_ACTOR_PREV, sizeof(Work) );
    if ( work != NULL )
    {
        GV_SetNamedActor( work, s14e_800C5B14, s14e_800C609C, "zako14.c" );
        s14e_800C62E0( work, name, where );
    }
    return (void *)work;
}
