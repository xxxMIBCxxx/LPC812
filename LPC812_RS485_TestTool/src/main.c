//=============================================================================
// Name        : LPC812_RS485_TestTool.c
// Author      : $(author)
// Version     :
// Copyright   : $(copyright)
// Description : main definition
//=============================================================================
#include "chip.h"
#include "StageMap.h"
#include "stdbool.h"
#include "string.h"

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// RS485通信コマンドを反転させる場合（通常運用はデファインを有効にすること）
#define _RS485_COMMAND_FLIP
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// LEDのON/OFFを逆にする場合（通常運用はデファインを有効にすること）
#define _LED_ON_REVESE
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#define RED_LED_PIN				(  7 )				// 赤色LED(P0_7)
#define GREEN_LED_PIN				(  6 )				// 緑色LED(P0_6)
#define START_BUTTON_PIN			(  0 )				// STARTボタン(P0_0)
#define UART0_TXD_PIN				( 15 )				// UART0 TXD(P0_15)
#define UART0_RXD_PIN				(  9 )				// UART0 RXD(P0_9)
#define UART0_DE_PIN				(  1 )				// UART0 DE(P0_1)

#define UART0_BAUDRATE				( 9600 )			// UART0 ボーレート
#define UART_RING_BUFFER_SIZE		( 64 )				// UART用リングバッファサイズ
#define UART_TIME_OUT_TIME			( 2 * 1000)			// 通信テストタイムアウト時間(ms)
#define TEST_START_WAIT_TIME		( 1000 )				// テスト前の待ち時間(ms)
														// ※直ぐ終わってしまうためわざとテストの時間をかけさせる


#define BUTTON_ON_TIME				(   50 )			// 押下となる時間(ms)
#define BUTTON_ON_LONG_TIME		( 1000 )			// 長押しとなる時間(ms)


#define STX						( 0x02 )			// STX
#define ETX						( 0x03 )			// ETX

#define TEST_RESULT_WAIT_TIME		( 1 * 1000 )		// テスト結果を必ず見せる時間(ms)
#define LED_ERROR_TIME				(  250 )			// エラー時の赤色LED点滅間隔(ms)


// 通信コマンドヘッダ
typedef struct
{
	uint8_t						Kubun;					// 区分
	uint8_t						Id[ 2 ];				// 機械ID
	uint8_t						Command;				// コマンド
	uint8_t						Param[ 4 ];				// パラメータ
	uint8_t						Length[ 3 ];			// データ長
} COMMAND_HEADER_TABLE;


// コマンド解析結果種別
typedef enum
{
	COMMAND_RESULT_OK = 0,		// コマンドまだ途中
	COMMAND_RESULT_ERROR,		// コマンド解析NG
	COMMAND_RESULT_SUCCESS		// コマンド解析OK
} COMMAND_RESULT_ENUM;


// RS485_Command種別
typedef enum
{
	RS485_COMMAND_STX = 0,		// STX待ち
	RS485_COMMAND_ETX,			// ETX待ち
} RS485_COMMAND_ENUM;


// ボタン種別
typedef enum
{
	BUTTON_OFF = 0,				// 押されていない
	BUTTON_ON  = 1,				// 押下
	BUTTON_ON_LONG = 2,			// 長押し
} BUTTON_ENUM;


// LED種別
typedef enum
{
	LED_WAIT_TEST = 0,			// テスト待ち（緑色LED：OFF , 赤色LED：OFF）
	LED_TEST,					// テスト中（緑色LED：点滅 　 ,   赤色LED：OFF）
	LED_RESULT_ERROR,			// テスト異常（緑色LED：OFF , 赤色LED：ON）				：応答なし（本来のエラー）

	LED_RESULT_ERROR_01,		// テスト異常01（緑色LED：OFF , 赤色LED：1回点滅）		：－
	LED_RESULT_ERROR_02,		// テスト異常02（緑色LED：OFF , 赤色LED：2回点滅）		：解析エラー
	LED_RESULT_ERROR_03,		// テスト異常03（緑色LED：OFF , 赤色LED：3回点滅）		：送信に失敗した
	LED_RESULT_ERROR_04,		// テスト異常04（緑色LED：OFF , 赤色LED：4回点滅）		：受信データ取得に失敗した
	LED_RESULT_ERROR_05,		// テスト異常05（緑色LED：OFF , 赤色LED：5回点滅）		：－
	LED_RESULT_ERROR_06,		// テスト異常06（緑色LED：OFF , 赤色LED：6回点滅）		：－
	LED_RESULT_ERROR_07,		// テスト異常07（緑色LED：OFF , 赤色LED：7回点滅）		：－
	LED_RESULT_ERROR_08,		// テスト異常08（緑色LED：OFF , 赤色LED：8回点滅）		：受信バッファフル

	LED_RESULT_SUCCESS,			// テスト正常（緑色LED：ON  , 赤色LED：OFF）
} LED_ENUM;


// グローバル変数構造体
typedef struct
{
	uint32_t					dwTimeOutTimer;						// タイムアウトタイマー(0xFFFFFFFFのときはカウントしない）

	// ボタン関連
	bool						bButton;							// 押下しているか
	uint32_t					dwButtonTime;						// 押下されてからの経過時間(ms)
	BUTTON_ENUM					eStartButton;						// テスト開始ボタンの状態

	// LED関連
	uint32_t					dwLedTimer;							// LED用タイマー(0xFFFFFFFFのときはカウントしない）
	uint8_t						LedCnt;								// LED点滅回数バックアップ用
	LED_ENUM					eLed;								// LEDの状態

	// UART0　ダブルバッファ関連
	RINGBUFF_T 					tTxRing;
	RINGBUFF_T 					tRxRing;
	uint8_t						szRxBuff[ UART_RING_BUFFER_SIZE ];
	uint8_t						szTxBuff[ UART_RING_BUFFER_SIZE ];

	RS485_COMMAND_ENUM 			eRs485_Command;						// RS485コマンド種別
	uint8_t						szRecv[ UART_RING_BUFFER_SIZE ];	// 受信データ
	uint32_t					dwCommandIndex;
	uint8_t						szCommand[ 256 ];					// 通信コマンド


} GROBAL_TABLE;
volatile GROBAL_TABLE		g_Grobal;

// 通信確認用送信データ
const uint8_t	g_szSendData[] = "<S01100  02000000100010         B6>";
//const uint8_t	g_szSendData[] = "<S01100  02000000100010         B5>";		// BCCエラー

// HEX変換用データ(strcmpi関数がないため…）
const uint8_t	g_szHex1[] = "0123456789ABCDEF";		// 大文字用
const uint8_t	g_szHex2[] = "0123456789abcdef";		// 小文字用



//-----------------------------------------------------------------------------
// 文字列のHEXを数値に変換する
//-----------------------------------------------------------------------------
uint32_t strHexConv( uint8_t *pszHex )
{
	uint32_t		dwRet = 0;
	bool			bFlag = false;
	uint32_t		i = 0;
	uint32_t		j = 0;
	uint8_t			ch = 0;


	for( i = 0 ; i < strlen(pszHex) ; i++ )
	{
		ch = pszHex[i];
		dwRet = dwRet << 4;

		// HEX変換用テーブルから一致する文字を探す
		bFlag = false;
		for( j = 0 ; j < strlen(g_szHex1) ; j++ )
		{
			if ( (ch == g_szHex1[j]) || (ch == g_szHex2[j]) )
			{
				dwRet += j;
				bFlag = true;
				break;
			}
		}

		// 見つからなかった場合
		if (bFlag == false)
		{
			return 0;
		}
	}

	return dwRet;
}


//-----------------------------------------------------------------------------
// コマンド解析処理
//-----------------------------------------------------------------------------
bool AnalyzeCommand(uint8_t *pszCommand, uint32_t dwCommand)
{
	COMMAND_HEADER_TABLE		*ptCommandHeader = NULL;
	uint8_t						szWork[ 10 ];
	int							Id = 0;
	int 						Length = 0;
	uint32_t					i = 0;
	uint8_t						bcc = 0x00;
	uint8_t						bcc_calc = 0x00;


	if (pszCommand == NULL)
	{
		return false;
	}
	ptCommandHeader = (COMMAND_HEADER_TABLE *)pszCommand;

	// 区分をチェック（'T'固定）
	if (ptCommandHeader->Kubun != 'T')
	{
		return false;
	}

	// 機械IDをチェック（1号機固定）
	memset(szWork, 0x00, sizeof(szWork));
	memcpy(szWork, ptCommandHeader->Id, 2);
	Id = atoi(szWork);
	if (Id != 1)
	{
		return false;
	}

	// コマンドをチェック（'1'固定）
	if (ptCommandHeader->Command != '1')
	{
		return false;
	}

	// データ長を取得
	memset(szWork, 0x00, sizeof(szWork));
	memcpy(szWork, &pszCommand[8], 3);
	Length = atoi(szWork);

	// BCCを取得
	memset(szWork, 0x00, sizeof(szWork));
	memcpy(szWork, &pszCommand[ (sizeof(COMMAND_HEADER_TABLE) + Length) ], 2 );
	bcc = strHexConv(szWork);

	// コマンドのBCCを算出
	bcc_calc = 0;
	for ( i = 0 ; i < (sizeof(COMMAND_HEADER_TABLE) + Length) ; i++ )
	{
		bcc_calc += pszCommand[i];
	}
	bcc_calc = ~bcc_calc;

	// BCCチェック
	if (bcc != bcc_calc)
	{
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// コマンド取得処理
//-----------------------------------------------------------------------------
COMMAND_RESULT_ENUM GetCommand(uint8_t *pszRecv, int iRecv)
{
	bool			bRet = false;
	int				i = 0;
	uint8_t			ch = 0x00;


	// 受信データ分ループ
	for( i = 0 ; i < iRecv ; i++ )
	{
#ifdef _RS485_COMMAND_FLIP
		ch = ~pszRecv[ i ];
#else
		ch = pszRecv[ i ];
#endif	// #ifdef _RS485_COMMAND_FLIP

		// RS485_Command種別
		if (g_Grobal.eRs485_Command == RS485_COMMAND_STX)
		{
			// STX ?
			if (ch == STX)
			{
				// ETX待ちにする
				g_Grobal.eRs485_Command = RS485_COMMAND_ETX;
			}
		}
		else if (g_Grobal.eRs485_Command == RS485_COMMAND_ETX)
		{
			// STX ?
			if (ch == STX)
			{
				// 既にSTXがきているのに、STXがきたのでエラー
				return COMMAND_RESULT_ERROR;
			}
			// ETX ?
			else if (ch == ETX)
			{
				// コマンド解析処理
				bRet = AnalyzeCommand(g_Grobal.szCommand, g_Grobal.dwCommandIndex);
				if (bRet == false)
				{
					return COMMAND_RESULT_ERROR;
				}

				// テスト正常
				return COMMAND_RESULT_SUCCESS;
			}
			else
			{
				// 受信データをRS485_コマンドバッファに格納
				g_Grobal.szCommand[ g_Grobal.dwCommandIndex ] = ch;
				g_Grobal.dwCommandIndex++;
			}
		}
	}

	return COMMAND_RESULT_OK;
}


//==[ テスト開始待ち状態 ]==============================================================
//-----------------------------------------------------------------------------
// テスト開始待ちの初期処理
//-----------------------------------------------------------------------------
void Init_WaitTest(void)
{
	// 無条件にUART0のリングバッファをクリアする
	RingBuffer_Flush(&g_Grobal.tRxRing);
	RingBuffer_Flush(&g_Grobal.tTxRing);

	// LED処理
	g_Grobal.eLed = LED_WAIT_TEST;
	LedProc( );

	return;
}


//-----------------------------------------------------------------------------
// テスト開始待ちの処理
//-----------------------------------------------------------------------------
void WaitTest(void)
{
	static BUTTON_ENUM 		eButton = BUTTON_OFF;

	// ウェイトをおく
	g_Grobal.dwTimeOutTimer = 0;
	while( 1 )
	{
		__WFI();

		// LED処理
		LedProc( );

		if (g_Grobal.dwTimeOutTimer >= TEST_RESULT_WAIT_TIME)
		{
			break;
		}
	}
	g_Grobal.dwTimeOutTimer = 0xFFFFFFFF;

	while( 1 )
	{
		__WFI();

		// LED処理
		LedProc( );

		// 受信データがあるときは、受信バッファをクリアする
		if (RingBuffer_GetCount(&g_Grobal.tRxRing) > 0)
		{
			RingBuffer_Flush(&g_Grobal.tRxRing);
		}

		// テスト開始ボタンが押下された場合
		if ((eButton == BUTTON_OFF) && (g_Grobal.eStartButton == BUTTON_ON))
		{
			g_Grobal.eLed = LED_TEST;
			LedProc( );

			// テスト中状態へ遷移
			SetStageMap(STAGE_MAP_TEST);
			break;
		}
	}

	return;
}


//-----------------------------------------------------------------------------
// テスト開始待ちの終了処理
//-----------------------------------------------------------------------------
void End_WaitTest(void)
{
	return;
}


//==[ テスト中状態 ]=================================================================
//-----------------------------------------------------------------------------
// テスト中の初期化処理
//-----------------------------------------------------------------------------
void Init_Test(void)
{
	// 無条件にUART0のリングバッファをクリアする
	RingBuffer_Flush(&g_Grobal.tRxRing);
	RingBuffer_Flush(&g_Grobal.tTxRing);

	// LED処理
	g_Grobal.eLed = LED_TEST;
	LedProc( );

	// RS485_Command種別を「STX待ち」にする
	g_Grobal.eRs485_Command = RS485_COMMAND_STX;

	// 通信コマンドバッファの初期化
	g_Grobal.dwCommandIndex = 0;
	memset( g_Grobal.szCommand, 0x00, sizeof(g_Grobal.szCommand));

	return;
}


//-----------------------------------------------------------------------------
// テスト中の処理
//-----------------------------------------------------------------------------
void Test(void)
{
	COMMAND_RESULT_ENUM		eRet = COMMAND_RESULT_ERROR;
	int						iSend = 0;
	int						iRecv = 0;
	int						iRead = 0;
	uint8_t					ch = 0x00;


	// テストが直ぐ終わってしまうため、わざとテストの時間をかけさせる
	g_Grobal.dwTimeOutTimer = 0;
	while( 1 )
	{
		__WFI();

		// LED処理
		LedProc( );

		if (g_Grobal.dwTimeOutTimer >= TEST_START_WAIT_TIME)
		{
			break;
		}
	}

	// コイン機へ状態要求を出す
	memset(g_Grobal.szCommand, 0x00, sizeof(g_Grobal.szCommand));
	for( int i = 0 ; i < strlen(g_szSendData) ; i++ )
	{
		ch = g_szSendData[i];

		if (ch =='<')
		{
			ch = STX;
		}
		else if (ch == '>')
		{
			ch = ETX;
		}

#ifdef _RS485_COMMAND_FLIP
		g_Grobal.szCommand[i] = ~ch;
#else
		g_Grobal.szCommand[i] = ch;
#endif	// 	#ifdef _RS485_COMMAND_FLIP
	}

	Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , UART0_DE_PIN);
	for( int i = 0 ; i < 100 ; i++ ) { __WFI(); }
	iSend = Chip_UART_SendBlocking(LPC_USART0, g_Grobal.szCommand, strlen(g_szSendData) );
	for( int i = 0 ; i < 100 ; i++ ) { __WFI(); }
	Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , UART0_DE_PIN);
	if (iSend != strlen(g_Grobal.szCommand))
	{
		// 送信が正しくできなかったため、エラーとする
		g_Grobal.eLed = LED_RESULT_ERROR_03;

		// テスト結果状態へ遷移
		SetStageMap(STAGE_MAP_RESULT_TEST);
		return;
	}

	// コイン機側から送信されるデータをチェックする
	g_Grobal.dwTimeOutTimer = 0;				// タイムアウト時間計測開始
	while( 1 )
	{
		LedProc( );

		// タイムアウト？
		if (g_Grobal.dwTimeOutTimer >= UART_TIME_OUT_TIME)
		{
			// 一定時間待っても、コイン機側から応答が返ってこないため、エラーとする（※本来のエラー）
			g_Grobal.eLed = LED_RESULT_ERROR;

			// テスト結果状態へ遷移
			SetStageMap(STAGE_MAP_RESULT_TEST);
			return;
		}

		// 受信バッファフル？
		if (RingBuffer_IsFull(&g_Grobal.tRxRing))
		{
			// ダブルバッファの調整が必要なため、エラーとする
			g_Grobal.eLed = LED_RESULT_ERROR_08;

			// テスト結果状態へ遷移
			SetStageMap(STAGE_MAP_RESULT_TEST);
			return;
		}

		// 受信データあり？
		iRecv = RingBuffer_GetCount(&g_Grobal.tRxRing);
		if ( iRecv > 0)
		{
			// 受信データ取得
			iRead = Chip_UART_ReadRB(LPC_USART0, &g_Grobal.tRxRing, g_Grobal.szRecv, iRecv );
			if (iRead != iRecv)
			{
				// 受信データ取得に失敗したので、エラーとする
				g_Grobal.eLed = LED_RESULT_ERROR_04;

				// テスト結果状態へ遷移
				SetStageMap(STAGE_MAP_RESULT_TEST);
				return;
			}

			// RS485_コマンド取得
			eRet = GetCommand( g_Grobal.szRecv, iRecv);
			switch (eRet) {
			case COMMAND_RESULT_OK:
				// RS485コマンドはまだ途中なので処理を続ける
				break;

			case COMMAND_RESULT_SUCCESS:
				// テスト正常
				g_Grobal.eLed = LED_RESULT_SUCCESS;

				// テスト結果状態へ遷移
				SetStageMap(STAGE_MAP_RESULT_TEST);
				return;

			case COMMAND_RESULT_ERROR:
			default:
				// コマンド解析エラー
				g_Grobal.eLed = LED_RESULT_ERROR_02;

				// テスト結果状態へ遷移
				SetStageMap(STAGE_MAP_RESULT_TEST);
				return;
			}
		}
	}

	return;
}


//-----------------------------------------------------------------------------
// テスト中の終了処理
//-----------------------------------------------------------------------------
void End_Test(void)
{
	// タイムアウト時間の計測を止める
	g_Grobal.dwTimeOutTimer = 0xFFFFFFFF;

	return;
}


//==[ テスト結果状態 ]=================================================================
//-----------------------------------------------------------------------------
// テスト結果の初期化処理
//-----------------------------------------------------------------------------
void Init_ResultTest(void)
{
	// 無条件にUART0のリングバッファをクリアする
	RingBuffer_Flush(&g_Grobal.tRxRing);
	RingBuffer_Flush(&g_Grobal.tTxRing);

	// LED処理（テスト中状態で結果(LED)を設定しているので、ここではLEDの処理だけを行う）
	LedProc( );

	return;
}


//-----------------------------------------------------------------------------
// テスト結果の処理
//-----------------------------------------------------------------------------
void ResultTest(void)
{
	static BUTTON_ENUM 		eButton = BUTTON_OFF;


	// かならずテスト結果を見せるため、ウェイトをおく
	g_Grobal.dwTimeOutTimer = 0;
	while( 1 )
	{
		__WFI();

		// LED処理
		LedProc( );

		if (g_Grobal.dwTimeOutTimer >= TEST_RESULT_WAIT_TIME)
		{
			break;
		}
	}
	g_Grobal.dwTimeOutTimer = 0xFFFFFFFF;

	while( 1 )
	{
		__WFI();

		// LED処理
		LedProc( );

		// 受信データがあるときは、受信バッファをクリアする
		if (RingBuffer_GetCount(&g_Grobal.tRxRing) > 0)
		{
			RingBuffer_Flush(&g_Grobal.tRxRing);
		}

		// テスト開始ボタンが押下された場合
		if ((eButton == BUTTON_OFF) && (g_Grobal.eStartButton == BUTTON_ON))
		{
			// テスト開始待ち状態へ遷移
			g_Grobal.eLed = LED_WAIT_TEST;
			LedProc( );

			SetStageMap(STAGE_MAP_WAIT_TEST);
			break;
		}
	}

	return;
}


//-----------------------------------------------------------------------------
// テスト結果の終了処理
//-----------------------------------------------------------------------------
void End_ResultTest(void)
{
	return;
}


//-----------------------------------------------------------------------------
// 1msタイマー割り込み
//-----------------------------------------------------------------------------
void MRT_IRQHandler(void)
{
	uint32_t		intf = 0;


	// 割り込みフラグの取得とクリアを行う
	intf = Chip_MRT_GetIntPending();
	Chip_MRT_ClearIntPending(intf);

	if (intf & MRT0_INTFLAG)
	{
		PushButtonCheck( );

		// タイムアウトタイマー
		if( g_Grobal.dwTimeOutTimer != 0xFFFFFFFF)
		{
			g_Grobal.dwTimeOutTimer++;
		}

		// LED用タイマー
		if (g_Grobal.dwLedTimer != 0xFFFFFFFF)
		{
			g_Grobal.dwLedTimer++;
		}
	}
}


//-----------------------------------------------------------------------------
// テスト開始ボタン押下判定処理
//-----------------------------------------------------------------------------
void PushButtonCheck(void)
{
	// テスト開始ボタン(Low)が押されている場合
	if (Chip_GPIO_GetPinState(LPC_GPIO_PORT, 0, START_BUTTON_PIN) == false)
	{
		g_Grobal.dwButtonTime++;

		// 2000ms以上押下している場合、「長押し」とする
		if (g_Grobal.dwButtonTime >= BUTTON_ON_LONG_TIME)
		{
			g_Grobal.eStartButton = BUTTON_ON_LONG;
		}
		else if (g_Grobal.dwButtonTime >= BUTTON_ON_TIME)
		{
			g_Grobal.eStartButton = BUTTON_ON;
		}
		else
		{
			g_Grobal.eStartButton = BUTTON_OFF;
		}
	}
	else
	{
		g_Grobal.eStartButton = BUTTON_OFF;
		g_Grobal.dwButtonTime = 0;
	}
}


//-----------------------------------------------------------------------------
// LED処理
//-----------------------------------------------------------------------------
void LedProc(void)
{
	static LED_ENUM 		eLed = LED_RESULT_SUCCESS;
	static uint32_t			dwToggleTimer = 0;				// 点滅間隔
	static uint8_t			LedCnt = 0;						// 点滅カウント


	//　LED種別に変化あり？
	if (eLed != g_Grobal.eLed)
	{
		eLed = g_Grobal.eLed;

		// LED種別によって処理を変更
		switch (eLed) {
		case LED_TEST:						// テスト中（緑色LED：点滅  , 赤色LED：OFF）
			g_Grobal.dwLedTimer = 0;
			dwToggleTimer = 200;
#ifndef _LED_ON_REVESE
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#else
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#endif	// #ifndef _LED_ON_REVESE
			break;

		case LED_RESULT_ERROR:				// テスト異常（緑色LED：OFF , 赤色LED：ON）
			g_Grobal.dwLedTimer = 0xFFFFFFFF;
#ifndef _LED_ON_REVESE
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#else
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#endif	// #ifndef _LED_ON_REVESE
			break;

		case LED_RESULT_ERROR_01:		// テスト異常01（緑色LED：OFF , 赤色LED：1回点滅）
		case LED_RESULT_ERROR_02:		// テスト異常02（緑色LED：OFF , 赤色LED：2回点滅）
		case LED_RESULT_ERROR_03:		// テスト異常03（緑色LED：OFF , 赤色LED：3回点滅）
		case LED_RESULT_ERROR_04:		// テスト異常04（緑色LED：OFF , 赤色LED：4回点滅）
		case LED_RESULT_ERROR_05:		// テスト異常05（緑色LED：OFF , 赤色LED：5回点滅）
		case LED_RESULT_ERROR_06:		// テスト異常06（緑色LED：OFF , 赤色LED：6回点滅）
		case LED_RESULT_ERROR_07:		// テスト異常07（緑色LED：OFF , 赤色LED：7回点滅）
		case LED_RESULT_ERROR_08:		// テスト異常08（緑色LED：OFF , 赤色LED：8回点滅）
			g_Grobal.dwLedTimer = 0;
			g_Grobal.LedCnt = 0;
			LedCnt = (2 * ((eLed - LED_RESULT_ERROR_01) + 1)) - 1;
			dwToggleTimer = LED_ERROR_TIME;
#ifndef _LED_ON_REVESE
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#else
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#endif	// #ifndef _LED_ON_REVESE
			break;

		case LED_RESULT_SUCCESS:			// テスト正常（緑色LED：ON  , 赤色LED：OFF）
			g_Grobal.dwLedTimer = 0xFFFFFFFF;
#ifndef _LED_ON_REVESE
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#else
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#endif	// #ifndef _LED_ON_REVESE
			break;

		case LED_WAIT_TEST:					// テスト待ち（緑色LED：OFF  , 赤色LED：OFF）
		default:
			g_Grobal.dwLedTimer = 0xFFFFFFFF;
#ifndef _LED_ON_REVESE
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#else
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , RED_LED_PIN);
			Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
#endif	// #ifndef _LED_ON_REVESE
			break;
		}
	}
	else
	{
		switch ( eLed) {
		case LED_TEST:					// テスト中（緑色LED：点滅  , 赤色LED：OFF）
			if (g_Grobal.dwLedTimer >= dwToggleTimer)
			{
				g_Grobal.dwLedTimer = 0;
				Chip_GPIO_SetPinToggle(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);
			}
			break;

		case LED_RESULT_ERROR_01:		// テスト異常01（緑色LED：OFF , 赤色LED：1回点滅）
		case LED_RESULT_ERROR_02:		// テスト異常02（緑色LED：OFF , 赤色LED：2回点滅）
		case LED_RESULT_ERROR_03:		// テスト異常03（緑色LED：OFF , 赤色LED：3回点滅）
		case LED_RESULT_ERROR_04:		// テスト異常04（緑色LED：OFF , 赤色LED：4回点滅）
		case LED_RESULT_ERROR_05:		// テスト異常05（緑色LED：OFF , 赤色LED：5回点滅）
		case LED_RESULT_ERROR_06:		// テスト異常06（緑色LED：OFF , 赤色LED：6回点滅）
		case LED_RESULT_ERROR_07:		// テスト異常07（緑色LED：OFF , 赤色LED：7回点滅）
		case LED_RESULT_ERROR_08:		// テスト異常08（緑色LED：OFF , 赤色LED：8回点滅）
			if (g_Grobal.dwLedTimer >= dwToggleTimer)
			{
				g_Grobal.dwLedTimer = 0;

				g_Grobal.LedCnt++;
				if (g_Grobal.LedCnt > LedCnt)
				{
					g_Grobal.LedCnt = 0;
					dwToggleTimer = 1000;
#ifndef _LED_ON_REVESE
					Chip_GPIO_SetPinOutHigh(LPC_GPIO_PORT, 0 , RED_LED_PIN);
#else
					Chip_GPIO_SetPinOutLow(LPC_GPIO_PORT, 0 , RED_LED_PIN);
#endif	// #ifndef _LED_ON_REVESE
				}
				else
				{
					dwToggleTimer = LED_ERROR_TIME;
					Chip_GPIO_SetPinToggle(LPC_GPIO_PORT, 0 , RED_LED_PIN);
				}
			}
			break;

		default:
			// 何も行わない
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// UART0 割込み処理
//-----------------------------------------------------------------------------
void UART0_IRQHandler(void)
{
	// 受信した場合は受信用バッファに格納し、送信用バッファにデータがある場合は送信をおこなう
	Chip_UART_IRQRBHandler( LPC_USART0, &g_Grobal.tRxRing, &g_Grobal.tTxRing );
}


//-----------------------------------------------------------------------------
// メイン処理
//-----------------------------------------------------------------------------
int main(void)
{
	// グローバル変数の初期化
	g_Grobal.dwTimeOutTimer = 0;
	g_Grobal.bButton = false;
	g_Grobal.dwButtonTime = 0xFFFFFFFF;
	g_Grobal.eStartButton = BUTTON_OFF;
	g_Grobal.eLed = LED_TEST;
	g_Grobal.dwLedTimer = 0;
	g_Grobal.LedCnt = 0;
	RingBuffer_Init( &g_Grobal.tRxRing, g_Grobal.szRxBuff, 1, UART_RING_BUFFER_SIZE );			// UART0で使用する受信用リングバッファを生成
	RingBuffer_Init( &g_Grobal.tTxRing, g_Grobal.szTxBuff, 1, UART_RING_BUFFER_SIZE );			// UART0で使用する送信用リングバッファを生成

	//▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼
    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();

	// UART0で使用するピンを設定
	Chip_Clock_EnablePeriphClock( SYSCTL_CLOCK_SWM );
	Chip_SWM_MovablePinAssign( SWM_U0_TXD_O, UART0_TXD_PIN );									// UART0_TXD
	Chip_SWM_MovablePinAssign( SWM_U0_RXD_I, UART0_RXD_PIN );									// UART0_RXD
	Chip_Clock_DisablePeriphClock( SYSCTL_CLOCK_SWM );

	// 使用するピンの設定
    Chip_GPIO_Init(LPC_GPIO_PORT);
    Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 0 , RED_LED_PIN);									// 赤色LED
    Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 0 , GREEN_LED_PIN);								// 緑色LED
    Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, 0, START_BUTTON_PIN);								// スイッチ
    //Chip_GPIO_SetMaskedPortValue(LPC_GPIO_PORT, 0, (0x00000001 << START_BUTTON_PIN) );
    Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, 0 , UART0_DE_PIN);									// UART0_DE

    // コイン機とシリアル通信ができるように設定する
	Chip_UART_Init( LPC_USART0 );																// UART0を有効にする
	Chip_UART_ConfigData( LPC_USART0, (UART_CFG_DATALEN_8 | UART_CFG_PARITY_EVEN | UART_CFG_STOPLEN_1) );
	Chip_Clock_SetUSARTNBaseClockRate((UART0_BAUDRATE * 8), true);								// UART関連のベースクロックを設定（※受信したデータが化ける場合は、8→16に変更してみてください）
	Chip_UART_SetBaud( LPC_USART0, UART0_BAUDRATE );											// UART0のボーレートを設定
	Chip_UART_Enable( LPC_USART0 );																// UART0の受信を有効にする
	Chip_UART_TXEnable( LPC_USART0 );															// UART0の送信を有効にする
	Chip_UART_IntEnable( LPC_USART0, UART_INTEN_RXRDY );										// UART0受信割込みを有効にする
	Chip_UART_IntDisable( LPC_USART0, UART_INTEN_TXRDY );										// UART0送信用割込みを無効にする（※UART API内部で有効／無効にしている）
	NVIC_EnableIRQ( UART0_IRQn );																// UART0割込みを有効にする

    // 1msタイマー開始
    Chip_MRT_Init();
    Chip_MRT_SetMode(LPC_MRT_CH0, MRT_MODE_REPEAT);
    Chip_MRT_SetInterval(LPC_MRT_CH0, (24000-1));			// 1ms
    Chip_MRT_SetEnabled(LPC_MRT_CH0);
    NVIC_EnableIRQ(MRT_IRQn);
    //▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲


    // テスト待ち状態へ遷移
    SetStageMap(STAGE_MAP_WAIT_TEST);
    while(1)
    {
    	// ステージマップ処理
    	StageMapProc( );
    }


    return 0;
}

