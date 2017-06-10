#ifndef PROTO_MSG_H
#define PROTO_MSG_H

#define P_MSG_INVALID_EOL	"Zprava musi byt ukoncena znakem LF nebo znaky CRLF."
#define P_MSG_IMPATIENT		"Je mozne poslat pouze jednu zpravu najednou. Pred odeslanim dalsi pockej na odpoved serveru. Anebo posilas znaky navic po znaku konce radku."
#define P_MSG_CONN_TOO_MANY	"Navazujes najednou vice spojeni, nez povoluje aktualni uloha."
#define P_MSG_CMD_NO_LETTER	"Zprava musi zacinat prikazem, ktery ma presne 4 znaky. Tyto znaky musi byt velka pismena."
#define P_MSG_CMD_EXTRA_CHARS	"Zprava musi zacinat prikazem, ktery ma presne 4 znaky. Za temito znaky musi nasledovat mezera nebo konec radku."
#define P_MSG_CMD_UNKNOWN	"Neznamy prikaz."
#define P_MSG_VAL_CONTAINS_NULL	"Zprava obsahuje znak \\0. Tento znak neni ve zprave povoleny."
#define P_MSG_VAL_TOO_LONG	"Zprava je prilis dlouha. Nechybi ti nekde znak konce radku?"
#define P_MSG_USER_EXPECTED	"Komunikace musi zacit prikazem USER."
#define P_MSG_USER_UNKNOWN	"Tento uzivatel neexistuje."
#define P_MSG_LEVL_EXPECTED	"Druhy prikaz komunikace musi byt LEVL."
#define P_MSG_LEVL_BAD_CHARS	"Kod levelu smi obsahovat jen mala pismena a cislice."
#define P_MSG_LEVL_NOT_MATCHING	"Jiz bezi spojeni pro jinou ulohu. Nelze resit dve ulohy najednou."
#define P_MSG_LEVL_UNKNOWN	"Uloha s timto kodem neexistuje."
#define P_MSG_TIMEOUT		"Vyprsel cas pro reseni teto ulohy."
#define P_MSG_CHAR_EXPECTED	"Tento prikaz ocekava jako parametr jeden znak."
#define P_MSG_2INT_EXPECTED	"Tento prikaz ocekava jako parametr dve nezaporna cisla."
#define P_MSG_EXTRA_PARAM	"Tento prikaz se vola bez parametru."
#define P_MSG_MAZE_NOT_AVAIL	"V teto uloze nelze ziskat data o celem bludisti. Pouzij prikaz WHAT."

#define A_MSG_WIN		"Vyborne! Blahoprejeme k dokonceni ulohy."
#define A_MSG_OUT_OF_MAZE	"Misto se nachazi mimo hraci plochu."
#define A_MSG_UNKNOWN_MOVE	"Neznamy smer pohybu."
#define A_MSG_WALL_HIT		"Tim smerem je zed."

#endif
