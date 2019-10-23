//=============================================================================
// StageMap.c
//
//  Created on: 2019/10/01
//      Author: M.Higuchi
//=============================================================================
#include "StageMap.h"



STAGE_MAP_ENUM					g_eStageMap = STAGE_MAP_WAIT_TEST;


//-----------------------------------------------------------------------------
// ステージマップ遷移
//-----------------------------------------------------------------------------
void SetStageMap(STAGE_MAP_ENUM eStageMap)
{
	PSTAGE_MAP_FUNC			pStageMapEndFunc = NULL;
	PSTAGE_MAP_FUNC			pStageMapInitFunc = NULL;


	if (eStageMap >= STAGE_MAP_MAX)
	{
		return;
	}

	// 遷移する前に現状のステージマップの終了処理を行う
	pStageMapEndFunc = g_tStageMap[g_eStageMap].pEndFund;
	if (pStageMapEndFunc != NULL)
	{
		pStageMapEndFunc();
	}

	g_eStageMap = eStageMap;

	// 遷移先のステージマップの初期処理を行う
	pStageMapInitFunc = g_tStageMap[eStageMap].pInitFunc;
	if (pStageMapInitFunc != NULL)
	{
		pStageMapInitFunc();
	}
}


//-----------------------------------------------------------------------------
// ステージマップ処理
//-----------------------------------------------------------------------------
void StageMapProc(void)
{
	PSTAGE_MAP_FUNC			pStageMapProcFunc = NULL;


	// 遷移先のメイン処理を行う
	pStageMapProcFunc = g_tStageMap[g_eStageMap].pProcFund;
	if (pStageMapProcFunc != NULL)
	{
		pStageMapProcFunc();
	}
}

