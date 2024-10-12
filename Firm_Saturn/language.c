
#include "main.h"


/******************************************************************************/

int lang_id = 0;

#define LANG_STR_NR 20

typedef struct _str_entry
{
	u32 hash;
	char *str;
	int index;
	struct _str_entry *next;
}STR_ENTRY;

static STR_ENTRY lang_str[LANG_STR_NR];
static STR_ENTRY *lang_str_table[64];


/******************************************************************************/


static char *lang_zhcn[LANG_STR_NR] = {
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
	"选择游戏分类",
};


static char *lang_en[LANG_STR_NR] = {
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
	"Select Game Category",
};


static char *lang_ptbr[LANG_STR_NR] = {
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
	"Selecionar categoria de jogo",
};


static char *lang_ja[LANG_STR_NR] = {
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
	"ジャンル選択",
};


static char *lang_fr[LANG_STR_NR] = {
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
	"Disque de jeu non détecté !",
	"Sélectionnez la catégorie de jeu",
};


static char *lang_ru[LANG_STR_NR] = {
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
	"Выберите категорию игры",
};


static char *lang_zhtw[LANG_STR_NR] = {
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
	"選擇遊戲分類",
};


static char *lang_de[LANG_STR_NR] = {
	"Spielauswahl(%d/%d)",
	"Dateiauswahl(%d/%d)",
	"Spiel wird geladen ......",
	"Laden des Spiels fehlgeschlagen! %d",
	"Datei wird geladen ......",
	"Laden der Datei fehlgeschlagen! %d",
	"Spiel auswählen",
	"System CD-Player",
	"Starte Spiel von CD",
	"Serielles Debug Menü",
	"Lade Binärdatei",
	"Firmware Update",
	"Update läuft... Konsole nicht ausschalten!",
	"Update fehlgeschlagen!",
	"Update erfolgreich! Konsole neu starten.",
	"SAROO Hauptmenü",
	"Prüfe CD ......",
	"Keine CD gefunden!",
	"Keine Spiele-CD!",
	"Spielkategorie auswählen",
};

static char *lang_es[LANG_STR_NR] = {
	"Seleccionar juego(%d/%d)",
	"Seleccionar archivo(%d/%d)",
	"Cargando juego ......",
	"¡Carga del juego fallida! %d",
	"Cargando archivo ......",
	"¡Carga del archivo fallida! %d",
	"Seleccionar juego",
	"Reproductor CD del Sistema",
	"Cargar disco de juego",
	"Consola de depuración serie UART",
	"Cargar binario",
	"Actualizar firmware",
	"Actualizando... ¡¡No apague la consola!!",
	"¡Actualización fallida!",
	"¡Actualización completada! ¡Ya puede reiniciar!",
	"SAROO Menú de arranque",
	"Comprobando disco .....",
	"¡Disco no encontrado!",
	"¡No es un disco de juego!",
	"Seleccionar categoría de juego",
};


static char *lang_it[LANG_STR_NR] = {
	"Seleziona gioco(%d/%d)",
	"Seleziona file(%d/%d)",
	"Caricamento gioco ......",
	"Caricamento gioco fallito! %d",
	"Caricamento file ......",
	"Caricamento file fallito! %d",
	"Seleziona gioco",
	"Lettore CD di sistema",
	"Carica disco di gioco",
	"Shell di debug UART",
	"Carica file binario",
	"Aggiornamento firmware",
	"Aggiornamento... Non spegnere la console!",
	"Aggiornamento fallito!",
	"Aggiornamento finito! Riavvia il sistema!",
	"Menu di avvio SAROO",
	"Controllo disco ......",
	"Nessun disco trovato!",
	"Nessun disco di gioco trovato!",
	"Seleziona categoria di gioco",
};


static char *lang_pl[LANG_STR_NR] = {
	"Wybierz grę(%d/%d)",
	"Wybierz plik(%d/%d)",
	"Uruchamianie gry ......",
	"Błąd uruchamiania gry! %d",
	"Wczytywanie pliku ......",
	"Błąd wczytywania pliku! %d",
	"Wybierz grę",
	"Systemowy CDPlayer",
	"Wczytaj dysk z grą",
	"Powłoka debugująca po UART",
	"Wczytaj dane binarne",
	"Aktualizacja Firmware",
	"Aktualizacja trwa... Nie wyłączaj konsoli!",
	"Błąd aktualizacji!",
	"Aktualizacja ukończona! Uruchom ponownie!",
	"Menu rozruchowe SAROO",
	"Sprawdzanie dysku ......",
	"Nie znaleziono dysku!",
	"Nie znaleziono płyty z grą!",
	"Wybierz kategorię gry",
};


static char *lang_swe[LANG_STR_NR] = {
	"Välj spel(%d/%d)",
	"Välj fil(%d/%d)",
	"Laddar spel ......",
	"Gick inte att starta spelet! %d",
	"Laddar fil ......",
	"Gick inte att ladda fil! %d",
	"Välj spel",
	"System CD-spelare",
	"Ladda spelskiva",
	"Seriellt felsökningsshell",
	"Ladda binär",
	"Uppdatera programvaran",
	"Uppdaterar... Stäng inte av maskinen.",
	"Uppdatering misslyckades!",
	"Uppdatering klar! Var god starta om maskinen.",
	"SAROO Boot Menu",
	"Kontrollerar skivan ......",
	"Hittade ingen skiva!",
	"Inte en giltig spel skiva!",
	"Välj spelkategori",
};


static char *lang_el[LANG_STR_NR] = {
	"Επίλεξε Παιχνίδι(%d/%d)",
	"Επίλεξε Αρχείο(%d/%d)",
	"Εκκίνηση Παιχνιδιού ......",
	"Εκκίνηση Παιχνιδιού Απέτυχε! %d",
	"Φόρτωση Αρχείου ......",
	"Φόρτωση Αρχείου Απέτυχε! %d",
	"Επίλεξε Παιχνίδι",
	"CD Player Συστήματος",
	"Φόρτωση Δίσκου Παιχνιδιού",
	"Σειριακή Κονσόλα Debug",
	"Φόρτωση Δυαδικού Αρχείου",
	"Ενημέρωση Λογισμικού",
	"Ενημέρωση... ΜΗΝ ΚΛΕΙΣΕΤΕ",
	"Ενημέρωση Απέτυχε!",
	"Ενημέρωση Ολοκληρώθηκε! Κάνε Επανακίνηση!",
	"Μενού Εκκίνησης SAROO",
	"Έλεγχος Δίσκου ......",
	"Δεν Βρέθηκε Δίσκος!",
	"Δεν Είναι Δίσκος Παιχνιδιού!",
	"Επίλεξε Κατηγορία Παιχνιδιού",
};


static char *lang_ro[LANG_STR_NR] = {
	"Selectați jocul(%d/%d)",
	"Selectați fişierul(%d/%d)",
	"Jocul porneşte ......",
	"Pornirea jocului a eşuat! %d",
	"Se încarcă fișierul ......",
	"Încărcarea fişierului a eşuat! %d",
	"Selectați jocul",
	"Interfață optică interactivă pentru sistem",
	"Încărcați discul jocului",
	"Depanați prin interfața serială",
	"Încărcați cod binar",
	"Actualizarea firmware-ului",
	"Se actualizează...Nu opriți alimentarea cu energie a aparatului!",
	"Actualizarea a eşuat!",
	"Actualizarea s-a încheiat! Vă rugăm să reporniți aparatul!",
	"Meniul SAROO de pornire",
	"Se verifică discul ......",
	"Nu s-a detectat nici nu disc!",
	"Nu s-a detectat un disc cu jocuri compatibil!",
	"Selectați categoria de jocuri",
};


static char **lang_list[] = {
	lang_zhcn,
	lang_en,
	lang_ptbr,
	lang_ja,
	lang_fr,
	lang_ru,
	lang_zhtw,
	lang_de,
	lang_es,
	lang_it,
	lang_pl,
	lang_swe,
	lang_el,
	lang_ro,
};

static const int lang_nr = sizeof(lang_list)/4;

static char **lang_cur;


/******************************************************************************/

static u32 str_hash(char *str)
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
	if(lang_id==0 || lang_id>=lang_nr)
		return str;

	u32 hash = str_hash(str);
	STR_ENTRY *entry = lang_str_table[hash&0xff];
	while(entry){
		if(hash==entry->hash){
			//printk("TT: %s(%08x) -> %d %s\n", str, hash, entry->index, lang_cur[entry->index]);
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

	if(lang_id>=lang_nr)
		lang_id = 0;
	lang_cur = lang_list[lang_id];
}


void lang_next(void)
{
	lang_id += 1;
	if(lang_id==lang_nr)
		lang_id = 0;

	lang_cur = lang_list[lang_id];
}

