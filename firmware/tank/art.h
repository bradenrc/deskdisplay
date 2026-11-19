// =====================================================================
//  ALL THE DRAWINGS  —  fish, creatures, scenery and the clock digits.
//  Change a drawing here, then check it on the PC before uploading:
//      python tools/fish_preview.py        (fish and creatures)
//      tools/viewer/viewer.exe             (the whole tank, live)
// =====================================================================
#pragma once

// ---------------------------------------------------------------- fish
// Every fish faces RIGHT (head on the right). In C strings "\\" is one backslash
// and "\xB0" is the ° eye. Each ART_ has a matching PAINT_ with the same layout:
// one letter per character saying what color it gets.
//   b body          l body, lighter (top / highlights)   d body, darker (underside)
//   f fin color     F fin color, lighter                 w white (belly, bands)
//   k dark marking  s shimmering scales                  e the eye (blinks)
//   space = body.
// The two colors each drawing uses (body and fin) are in settings.h.
static const char* ART_CLASSIC[] = { "><((((\xB0>" };
static const char* PAINT_CLASSIC[] = { "ffsssseb" };

static const char* ART_MINNOW[] = { "><\xB0>" };
static const char* PAINT_MINNOW[] = { "fbel" };

static const char* ART_DELUXE[] = {
  "\\`. _,-._",
  " >=((((( \xB0`>",
  "/,' `'-'" };
static const char* PAINT_DELUXE[] = {
  "ff  fFFff",
  " fksssss ebl",
  "ff  fffd" };

static const char* ART_CLOWN[] = {
  "\\ ,-|-|-.",
  " >( | | \xB0>",
  "/ `-|-|-'" };
static const char* PAINT_CLOWN[] = {
  "k lkwkwkl",
  " ks w w el",
  "k ddwdwdd" };

static const char* ART_ANGEL[] = {
  "    /|",
  "\\ ,'|(`.",
  " >=||( \xB0>",
  "/ `.|(,'",
  "    \\|" };
static const char* PAINT_ANGEL[] = {
  "    ff",
  "f llkbll",
  " ffkkb el",
  "f ddkbdd",
  "    ff" };

static const char* ART_GROUPER[] = {
  "   _.----._",
  "|\\' o  o   `.",
  "|/( o  o  \xB0 =",
  "   `-.___.-'" };
static const char* PAINT_GROUPER[] = {
  "   llllllll",
  "bbl f  f   ll",
  "bbb f  f  e b",
  "   ddddddddd" };

static const char* ART_PUFFER[] = {
  " ,^^^.",
  "<(. \xB0)>",
  " `vvv'" };
static const char* PAINT_PUFFER[] = {
  " fFFFf",
  "fbd ebf",
  " dwwwd" };
static const char* ART_PUFFED[] = {
  " \\ ^ ^ ^ /",
  "<( .  . \xB0)o",
  " / v v v \\" };
static const char* PAINT_PUFFED[] = {
  " f F F F f",
  "fb d  d ebl",
  " f w w w f" };

static const char* ART_LIONFISH[] = {   // white spiky fins, striped body
  " \\ \\|/ /",
  " >={|{|( \xB0>",
  " / /|\\ \\" };
static const char* PAINT_LIONFISH[] = {
  " F FFF F",
  " fkwkwkw el",
  " F FFF F" };

static const char* ART_BETTA[] = {      // small body, big flowing tail
  " \\`\\,_",
  " }}}=(((\xB0>",
  " /,/`" };
static const char* PAINT_BETTA[] = {
  " FFFFf",
  " FFFfsssel",
  " FFFf" };

// ---------------------------------------------------------------- other creatures
// Not fish, but drawn the same way (same paint letters).
static const char* ART_SNAIL[] = {
  " .-.  ,,",
  "(@  )_/" };
static const char* PAINT_SNAIL[] = {
  " bbb  FF",
  "bbbbbFF" };
static const char* ART_SNAIL_TUCKED[] = {   // hiding in its shell while the crab is close
  " .-.",
  "(@  )" };


// A green moray: long body with one fin running along its back, thick neck, a big round
// eye on its head, a smooth snout, and a jaw that opens and closes (that's how morays
// breathe; see EEL_MOUTH). Faces RIGHT.
static const char* ART_EEL[] = {            // mouth open: the lower jaw hangs down
  "   _,~~~~~~~~~-.__",
  " ~(___________   o   ",               // 'o' = the eye; spaces keep the snout's length
  "             `-.\\vv/'",
  "               `--'" };
static const char* ART_EEL_SHUT[] = {       // mouth shut: the lower jaw tucked under the head
  "   _,~~~~~~~~~-.__",
  " ~(___________   o   ",               // 'o' = the eye; spaces keep the snout's length
  "",                                      // the jaw is drawn as a solid shape (tank.h), no outline
  "" };
static const char* PAINT_EEL_SHUT[] = {
  "   FFFFFFFFFFFFlll",
  " fbbbbbbbbbbbbb  e   ",
  "",
  "" };
static const char* PAINT_EEL[] = {
  "   FFFFFFFFFFFFlll",
  " fbbbbbbbbbbbbb  e   ",
  "             dddwwwd",
  "               dddd" };

static const char* ART_TURTLE[] = {         // the other rare visitor (faces LEFT)
  "       ____",
  "  _  .'_/\\_`.  __",
  " (\xB0`/ /\\__/\\ \\'_/",
  "  `-\\_\\/__\\/_/-.",
  "     /_/   \\_\\" };
static const char* PAINT_TURTLE[] = {
  "       ffff",
  "  b  ffffffff  bb",
  " bebf ffffff fbbb",
  "  bbbffffffffbbb",
  "     ddd   ddd" };

static const char* ART_WHALE[] = {   // a rare visitor, far in the background
  "            _.--''''''--._",
  " \\`.    _.-'              `-.",
  "  )  :-'     .  .        \xB0   \\",
  " /,.'  `-._                 _/",
  "             `--..______..-'" };
static const char* PAINT_WHALE[] = {
  "            llllllllllllll",
  " ff fffllll              llll",
  "  f  ffb     w  w        e   b",
  " ff ffff ddd                dd",
  "             ddddddddddddddd" };

// The crab: two leg positions (it alternates them while walking). Its claws are the
// "(\/)" at each end of the top row; the code swaps in "(||)" when one snaps shut.
static const char* ART_CRAB[] = {
  "(\\/)  \xB0 \xB0  (\\/)",
  " \\\\_.-\"\"\"-._//",
  "   (_______)",
  "  /' /' '\\ '\\" };
static const char* ART_CRAB_STEP[] = {
  "(\\/)  \xB0 \xB0  (\\/)",
  " \\\\_.-\"\"\"-._//",
  "   (_______)",
  "  '/ '/ \\' \\'" };
static const char* PAINT_CRAB[] = {
  "FFFF  e e  FFFF",
  " FFblllllllbFF",
  "   bbbbbbbbb",
  "  bb bb bb bb" };

static const char* JELLY[] = {
  "  .-\"-.",
  " /     \\",
  " '-...-'",
  "  ( ( )",
  "   ) )" };
static const int JELLY_COLS = 9, JELLY_ROWS = 5;

// ---------------------------------------------------------------- scenery (part of the background)
// Scenery is drawn once into the background picture, so it costs nothing while the tank
// runs. It can be at most 5 rows tall. These use the same paint letters as the fish.
static const char* ART_CASTLE[] = {
  "        |>",
  "  n_n   |   n_n",
  "  |#|__/^\\__|#|",
  "  | |  [#]  | |",
  " _|_|__|_|__|_|_" };
static const char* PAINT_CASTLE[] = {
  "        lf",
  "  lll   l   lll",
  "  bkbbbbbbbbbkb",
  "  b b  bkb  b b",
  " ddddddddddddddd" };

static const char* ART_CHEST[] = {      // treasure chest, closed
  " .=====.",
  " |==O==|",
  " |_____|" };
static const char* PAINT_CHEST[] = {
  " lllllll",
  " bbbFbbb",
  " ddddddd" };
static const char* ART_CHEST_OPEN[] = { // ...and with the lid up (it opens now and then)
  " /=====/",
  " '-----'",
  " |*$*$*|",
  " |_____|" };
static const char* PAINT_CHEST_OPEN[] = {
  " lllllll",
  " ddddddd",
  " bFFFFFb",
  " ddddddd" };

static const char* ART_FAN_CORAL[] = {
  " \\|/ \\|/",
  " \\\\|//|/",
  "  \\\\|//",
  "   \\|/" };
static const char* PAINT_FAN_CORAL[] = {
  " FbF FbF",
  " bbbbbbb",
  "  bbbbb",
  "   ddd" };

static const char* ART_TUBE_CORAL[] = {
  " o  o",
  " |o ||o",
  " || |||",
  " ||_|||" };
static const char* PAINT_TUBE_CORAL[] = {
  " F  F",
  " bF bbF",
  " bb bbb",
  " dddddd" };

// A rock with a cave in its right side; the eel lives in it. The mouth (under the long
// overhang in the second row, right of the back wall) is shaded dark when the background is drawn.
static const char* ART_CAVE[] = {
  "    _.-''''-._",
  "  .'.:'.:.:'.:`-----.",
  " /:.:'.:.:'./",
  "|.:'.:.:'.:|",
  "\\_:._.:_._:|" };
static const char* PAINT_CAVE[] = {
  "    llllllllll",
  "  lldldldldldllllllll",
  " bdldldldldlb",
  "bdldldldldbb",
  "bddddddddddb" };
static const char* BRAIN[] = { "  .--.", " (@@@@)" };

// ---------------------------------------------------------------- clock
// Big clock digits, three characters wide and three rows tall (0-9).
static const char* CLOCK_DIGITS[10][3] = {
  { " _ ", "| |", "|_|" }, { "   ", "  |", "  |" }, { " _ ", " _|", "|_ " }, { " _ ", " _|", " _|" },
  { "   ", "|_|", "  |" }, { " _ ", "|_ ", " _|" }, { " _ ", "|_ ", "|_|" }, { " _ ", "  |", "  |" },
  { " _ ", "|_|", "|_|" }, { " _ ", "|_|", " _|" } };
