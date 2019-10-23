//=============================================================================
// StageMap.h
//
//  Created on: 2019/10/01
//      Author: M.Higuchi
//=============================================================================
#ifndef STAGEMAP_H_
#define STAGEMAP_H_

#include "chip.h"


// ステージマップ処理関数定義
typedef void (*PSTAGE_MAP_FUNC) (void);

// ステージマップ構造体
typedef struct
{
	PSTAGE_MAP_FUNC				pInitFunc;		// 初期処理関数
	PSTAGE_MAP_FUNC				pProcFund;		// 処理関数
	PSTAGE_MAP_FUNC				pEndFund;		// 終了処理関数
} STAGE_MAP_TABLE;


// ▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽▼▽
extern void Init_WaitTest(void);
extern void WaitTest(void);
extern void End_WaitTest(void);
extern void Init_Test(void);
extern void Test(void);
extern void End_Test(void);
extern void Init_ResultTest(void);
extern void ResultTest(void);
extern void End_ResultTest(void);

// ステージマップの種別
typedef enum
{
	STAGE_MAP_WAIT_TEST = 0,		// テスト待ち
	STAGE_MAP_TEST,					// テスト中
	STAGE_MAP_RESULT_TEST,			// テスト結果
	STAGE_MAP_MAX					// == 終端 ==
} STAGE_MAP_ENUM;


// ステージマップ処理定義
static STAGE_MAP_TABLE g_tStageMap[STAGE_MAP_MAX] =
{
	{ Init_WaitTest		, WaitTest			, NULL				},	// テスト待ち(STAGE_MAP_WAIT_TEST)
	{ Init_Test			, Test				, End_Test			},	// テスト中(STAGE_MAP_TEST)
	{ Init_ResultTest	, ResultTest		, NULL				}	// テスト結果(STAGE_MAP_RESULT_TEST)
};
// ▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△▲△


//-----------------------------------------------------------------------------
// ステージマップ遷移
//-----------------------------------------------------------------------------
void SetStageMap(STAGE_MAP_ENUM eStageMap);


//-----------------------------------------------------------------------------
// ステージマップ処理
//-----------------------------------------------------------------------------
void StageMapProc(void);


#endif // #ifndef STAGEMAP_H_
