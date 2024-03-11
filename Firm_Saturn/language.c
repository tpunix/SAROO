
#include "main.h"


/******************************************************************************/

int lang_id = 0;

#define LANG_STR_NR 19
#define LANG_NR 8

typedef struct _str_entry
{
	u32 hash;
	char *str;
	int index;
	struct _str_entry *next;
}STR_ENTRY;

STR_ENTRY lang_str[LANG_STR_NR];
STR_ENTRY *lang_str_table[64];


/******************************************************************************/


char *lang_zhcn[LANG_STR_NR] = {
	"选择游戏(%d/%d)",
	"选择文件(%d/%d)",
	"游戏启动中......",
	"游戏启动失败! %d",
	"加载文件中......",
	"文件加载失败! %d",
	"选择游戏",
	"系统CD播放器",
	"运行光盘游戏",
	"串口调试工具",
	"运行二进制文件",
	"固件升级",
	"升级中,请勿断电...",
	"升级失败!",
	"升级完成,请重新开机!",
	"SAROO Boot Menu",
	"检查光盘中......",
	"未发现光盘!",
	"不是游戏光盘!",
};


char *lang_en[LANG_STR_NR] = {
	"Select Game(%d/%d)",
	"Select File(%d/%d)",
	"Game Booting ......",
	"Game Boot Failed! %d",
	"Loading File ......",
	"Load File Failed! %d",
	"Select Game",
	"System CDPlayer",
	"Load Game Disc",
	"Serial Debug Shell",
	"Load Binary",
	"Firm Update",
	"Updating... Don't PowerOff!",
	"Update Failed!",
	"Update Finish! Please PowerOn again!",
	"SAROO Boot Menu",
	"Checking Disc ......",
	"No Disc Found!",
	"Not a Game Disc!",
};


char *lang_ptbr[LANG_STR_NR] = {
	"Selecionar Jogo(%d/%d)",
	"Selecionar Arquivo(%d/%d)",
	"Inicializando Jogo ......",
	"Erro ao inicializar o jogo! %d",
	"Carregando arquivo ......",
	"Erro ao carregar o arquivo! %d",
	"Selecionar Jogo",
	"Tocador de CD do Sistema",
	"Carregar o Disco do Jogo",
	"Shell de Depuração serial",
	"Carregando Binário",
	"Atualizar a Firmware",
	"Atualizando... Não desligue o sistema!",
	"Erro ao atualizar!",
	"Finalizado, reinicie o sistema!",
	"SAROO Menu de Inicialização",
	"Verificando o disco ......",
	"Sem CD!",
	"Não é um disco de jogo!",
};


char *lang_ja[LANG_STR_NR] = {
	"ゲームリスト(%d/%d)",
	"ファイルリスト(%d/%d)",
	"ロード中......",
	"ゲームを起動できない！ %d",
	"ファイルをロード中......",
	"ファイルを起動できない！ %d",
	"ゲームリスト",
	"CDプレイヤー",
	"CDROMを起動",
	"シリアルポートデバッグツール",
	"バイナリファイルを起動",
	"ファームウェアのアップデート",
	"アップデート中、電源を切らないでください...",
	"アップデート失敗！",
	"アップデート完了、再起動してください！",
	"SAROO ブートメニュー",
	"ディスクを確認しています......",
	"ディスクが入っていません",
	"ゲームディスクではありません",
};


char *lang_fr[LANG_STR_NR] = {
	"Choisir un jeu (%d/%d)",
	"Choisir un fichier (%d/%d)",
	"Démarrage du jeu...",
	"Erreur lors du démarrage du jeu ! %d",
	"Chargement du fichier...",
	"Erreur de chargement du fichier ! %d",
	"Choisir un jeu",
	"Lecteur CD",
	"Charger un jeu CD-ROM",
	"Console de débogage série",
	"Charger un binaire",
	"Mise à jour du firmware",
	"Mise à jour... Ne pas éteindre le système !",
	"Erreur de mise à jour !",
	"Mise à jour terminée ! Redémarrez le système !",
	"SAROO Menu de démarrage",
	"Vérification du disque...",
	"Aucun disque trouvé !",
	"Disque de jeu non détecté !"
};


char *lang_ru[LANG_STR_NR] = {
	"Выбор игры (%d/%d)",
	"Выбор файла (%d/%d)",
	"Запуск игры ...",
	"Сбой запуска игры! %d",
	"Запуск файла ...",
	"Сбой запуска файла! %d",
	"Выбрать игру",
	"Аудио CD плеер",
	"Запустить игру с CD",
	"Инструмент отладки",
	"Запустить исп. файл",
	"Обновление ПО",
	"Обновление, не отключайте ...",
	"Сбой обновления!",
	"Обновлено! Требуется перезапуск!",
	"Главное меню SAROO",
	"Проверка диска ...",
	"Диск не найден!",
	"Не игровой диск!",
};


char *lang_zhtw[LANG_STR_NR] = {
	"選擇遊戲(%d/%d)",
	"選擇檔案(%d/%d)",
	"啟動遊戲中......",
	"遊戲啟動失敗! %d",
	"載入檔案中......",
	"檔案載入失敗! %d",
	"選擇遊戲",
	"系統CD播放器",
	"執行遊戲光碟",
	"UART除錯工具",
	"執行應用程式擴充檔案",
	"升級韌體",
	"韌體升級中,請勿斷電...",
	"韌體升級失敗!",
	"韌體升級完成,請重新開機!",
	"SAROO 主選單",
	"檢查光碟中......",
	"未發現光碟!",
	"不是遊戲光碟!",
};


char **lang_cur;


/******************************************************************************/

u32 str_hash(char *str)
{
	int len = strlen(str);
	u32 hash = 5381;
	int i;

	for(i=0; i<len; i++){
		hash = ((hash<<5)+hash) ^ (u8)str[i];
	}

	return hash;
}

char *TT(char *str)
{
	if(lang_id==0 || lang_id>=LANG_NR)
		return str;

	u32 hash = str_hash(str);
	STR_ENTRY *entry = lang_str_table[hash&0xff];
	while(entry){
		if(hash==entry->hash){
			return lang_cur[entry->index];
		}
		entry = entry->next;
	}

	return str;
}

/******************************************************************************/


void lang_init(void)
{
	int i;

	memset(lang_str_table, 0, sizeof(lang_str_table));
	memset(lang_str, 0, sizeof(lang_str));

	for(i=0; i<LANG_STR_NR; i++){
		lang_str[i].str = lang_zhcn[i];
		lang_str[i].hash = str_hash(lang_zhcn[i]);
		lang_str[i].index = i;

		int t = (lang_str[i].hash)&0xff;
		if(lang_str_table[t]){
			lang_str[i].next = lang_str_table[t];
		}
		lang_str_table[t] = &lang_str[i];

		//printk("%2d: %08x %s\n", i, lang_str[i].hash, lang_str[i].str);
	}

	lang_cur = NULL;
	if(lang_id==1){
		lang_cur = lang_en;
	}else if(lang_id==2){
		lang_cur = lang_ptbr;
	}else if(lang_id==3){
		lang_cur = lang_ja;
	}else if(lang_id==4){
		lang_cur = lang_fr;
	}else if(lang_id==5){
		lang_cur = lang_ru;
	}else if(lang_id==6){
		lang_cur = lang_zhtw;
	}
}


void lang_next(void)
{
	lang_id += 1;
	if(lang_id>6)
		lang_id = 0;

	lang_init();
}

