#include "chip.h"
#include "stdbool.h"


// UART0
#define LPC_USART				( LPC_USART0 )
#define LPC_IRQNUM      		( UART0_IRQn )
#define LPC_UARTHNDLR   		( UART0_IRQHandler )
#define SWM_UART_TXD_O			( SWM_U0_TXD_O )
#define SWM_UART_RXD_I			( SWM_U0_RXD_I )


#define UART_BAUDRATE			( 9600 )				// UART ボーレート
#define UART_RING_BUFFER_SIZE	( 64 )					// UART用リングバッファサイズ
#define UART_TXD_PIN			(  4 )					// UART TXD(P0_4)
#define UART_RXD_PIN			(  0 )					// UART RXD(P0_0)



// グローバル変数構造体
typedef struct
{
	// UART　ダブルバッファ関連
	RINGBUFF_T 				tTxRing;
	RINGBUFF_T 				tRxRing;
	uint8_t					szRxBuff[ UART_RING_BUFFER_SIZE ];
	uint8_t					szTxBuff[ UART_RING_BUFFER_SIZE ];
} GROBAL_TABLE;
volatile GROBAL_TABLE		g_Grobal;

const char inst1[] = "LPC8xx UART example using ring buffers\r\n";
const char inst2[] = "Press a key to echo it back or ESC to quit\r\n";

//-----------------------------------------------------------------------------
// UART 割込み処理
//-----------------------------------------------------------------------------
void LPC_UARTHNDLR(void)
{
	// 受信した場合は受信用バッファに格納し、送信用バッファにデータがある場合は送信をおこなう
	Chip_UART_IRQRBHandler( LPC_USART, &g_Grobal.tRxRing, &g_Grobal.tTxRing );
}


//-----------------------------------------------------------------------------
// メイン処理
//-----------------------------------------------------------------------------
int main(void)
{
	uint8_t 	key = 0x00;
	int 		bytes = 0;


	// グローバル変数の初期化
	RingBuffer_Init( &g_Grobal.tRxRing, g_Grobal.szRxBuff, 1, UART_RING_BUFFER_SIZE );		// UARTで使用する受信用リングバッファを生成
	RingBuffer_Init( &g_Grobal.tTxRing, g_Grobal.szTxBuff, 1, UART_RING_BUFFER_SIZE );		// UARTで使用する送信用リングバッファを生成

	//▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼
    // Read clock settings and update SystemCoreClock variable
    SystemCoreClockUpdate();

	// UART0で使用するピンを設定
	Chip_Clock_EnablePeriphClock( SYSCTL_CLOCK_SWM );
	Chip_SWM_MovablePinAssign( SWM_UART_TXD_O, UART_TXD_PIN );								// UART_TXD
	Chip_SWM_MovablePinAssign( SWM_UART_RXD_I, UART_RXD_PIN );								// UART_RXD
	Chip_Clock_DisablePeriphClock( SYSCTL_CLOCK_SWM );

    // コイン機とシリアル通信ができるように設定する
	Chip_UART_Init( LPC_USART );																		// UART0を有効にする
	Chip_UART_ConfigData( LPC_USART, (UART_CFG_DATALEN_8 | UART_CFG_PARITY_NONE | UART_CFG_STOPLEN_1) );
	Chip_Clock_SetUSARTNBaseClockRate((UART_BAUDRATE * 8), true);							// UARTのベースクロックを設定（※受信したデータが化ける場合は、8→16に変更してみてください）
	Chip_UART_SetBaud( LPC_USART, UART_BAUDRATE );											// UARTのボーレートを設定
	Chip_UART_Enable( LPC_USART );															// UARTの受信を有効にする
	Chip_UART_TXEnable( LPC_USART );														// UARTの送信を有効にする
	Chip_UART_IntEnable( LPC_USART, UART_INTEN_RXRDY );										// UART受信割込みを有効にする
	Chip_UART_IntDisable( LPC_USART, UART_INTEN_TXRDY );									// UART送信用割込みを無効にする（※UART API内部で有効／無効にしている）
	NVIC_EnableIRQ( LPC_IRQNUM );															// UART割込みを有効にする
    //▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲

	// 開始時、メッセージを出力する
	Chip_UART_SendBlocking(LPC_USART, inst1, sizeof(inst1) - 1);
	Chip_UART_SendRB(LPC_USART, &g_Grobal.tTxRing, inst2, sizeof(inst2) - 1);

	// ESC(ASC 27)キーを押下するまでループ
	key = 0;
	while (key != 27)
	{
		// １文字受信
		bytes = Chip_UART_ReadRB(LPC_USART, &g_Grobal.tRxRing, &key, 1);
		if (bytes > 0)
		{
			// １文字送信
			Chip_UART_SendRB(LPC_USART, &g_Grobal.tTxRing, (const uint8_t *) &key, 1);
		}
	}

	// UARTの後片付け
	NVIC_DisableIRQ(LPC_IRQNUM);
	Chip_UART_DeInit(LPC_USART);

    return 0;
}

