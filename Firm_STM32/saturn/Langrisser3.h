



#define MSF_TO_FAD(m,s,f) ((m * 4500) + (s * 75) + f)


/**********************************************/


char l3_name[] =  "/SAROOO/ISO/l3.iso";

int l3_track_fad[22] = {
	MSF_TO_FAD( 0,  0,  0), // Track 01 MODE1/2352
	MSF_TO_FAD( 7, 20,  5), // Track 02 MODE2/2352
	MSF_TO_FAD(19, 49, 72), // Track 03 AUDIO
	MSF_TO_FAD(20, 24, 52), // Track 04 AUDIO
	MSF_TO_FAD(21,  5, 69), // Track 05 AUDIO
	MSF_TO_FAD(22,  7, 70), // Track 06 AUDIO
	MSF_TO_FAD(22, 48, 70), // Track 07 AUDIO
	MSF_TO_FAD(23, 31, 13), // Track 08 AUDIO
	MSF_TO_FAD(24,  7, 51), // Track 09 AUDIO
	MSF_TO_FAD(24, 42, 30), // Track 10 AUDIO
	MSF_TO_FAD(25, 35, 64), // Track 11 AUDIO
	MSF_TO_FAD(26, 16, 56), // Track 12 AUDIO
	MSF_TO_FAD(26, 55,  3), // Track 13 AUDIO
	MSF_TO_FAD(27, 29, 62), // Track 14 AUDIO
	MSF_TO_FAD(28,  8, 39), // Track 15 AUDIO
	MSF_TO_FAD(28, 58, 31), // Track 16 AUDIO
	MSF_TO_FAD(29, 46,  2), // Track 17 AUDIO
	MSF_TO_FAD(30, 51,  4), // Track 18 AUDIO
	MSF_TO_FAD(31, 43, 21), // Track 19 AUDIO
	MSF_TO_FAD(32, 23, 24), // Track 20 AUDIO
	MSF_TO_FAD(33,  4, 60), // Track 21 AUDIO
	MSF_TO_FAD(34, 53,  9), // Track 22 AUDIO
};

int l3_track_num = 22;

/**********************************************/

char sakura1_name[] = "/SAROOO/ISO/SAKURA_CD1.bin";

int sakura1_track_fad[] = {
	MSF_TO_FAD( 0,  0,  0), // Track 01 MODE1/2352
	MSF_TO_FAD(60,  6, 73), // Track 02 AUDIO
};

int sakura1_track_num = 2;

/**********************************************/

