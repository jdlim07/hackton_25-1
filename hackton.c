#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // 시간 및 날짜 관련 함수
#include <ctype.h> // 문자열 처리 (예: tolower)

// 최대 길이를 정의하여 버퍼 오버플로우 방지
#define MAX_ID_LEN 50
#define MAX_PW_LEN 50
#define MAX_QUESTION_LEN 200
#define MAX_ANSWER_LEN 500
#define MAX_CATEGORY_LEN 50
#define MAX_DATE_LEN 15 // YYYY-MM-DD\0
#define MAX_USERS 100
#define MAX_RECORDS 1000
#define MAX_QUESTIONS 50
#define MAX_DARES 50
#define MAX_DARE_ATTEMPTS_PER_DAY 5

// 파일명 정의
#define USERS_FILE "users.txt"
#define RECORDS_FILE "records.txt"
#define TRUTH_QUESTIONS_FILE "truth_questions.txt"
#define DARE_CHALLENGES_FILE "dare_challenges.txt"

// --- 구조체 정의 ---

// 사용자 정보 구조체
typedef struct {
    char id[MAX_ID_LEN];
    char password[MAX_PW_LEN];
    int coins;
    char lastTruthDate[MAX_DATE_LEN]; // Truth 완료한 마지막 날짜
    char lastDareDate[MAX_DATE_LEN];  // Dare 시도한 마지막 날짜
    int dareAttemptsToday;            // 오늘 Dare 시도 횟수
} User;

// Truth 질문 구조체
typedef struct {
    int id;
    char question[MAX_QUESTION_LEN];
    int used; // 해당 세션에서 이미 사용된 질문인지 표시 (0: No, 1: Yes)
} TruthQuestion;

// Dare 도전 구조체
typedef struct {
    int id;
    char category[MAX_CATEGORY_LEN];
    char challenge[MAX_QUESTION_LEN];
} DareChallenge;

// 사용자 기록 구조체
typedef struct {
    char userId[MAX_ID_LEN];
    char date[MAX_DATE_LEN];
    int type; // 0: Truth, 1: Dare
    int contentId; // TruthQuestion 또는 DareChallenge의 ID
    char response[MAX_ANSWER_LEN]; // Truth 답변 또는 Dare 결과 (Complete/Fail)
    int coinsEarned; // Dare로 획득한 코인 (Truth는 0)
} UserRecord;


// --- 전역 변수 ---
User users[MAX_USERS];
int numUsers = 0;
User currentUser; // 현재 로그인한 사용자

TruthQuestion truthQuestions[MAX_QUESTIONS];
int numTruthQuestions = 0;

DareChallenge dareChallenges[MAX_DARES];
int numDareChallenges = 0;

UserRecord userRecords[MAX_RECORDS];
int numRecords = 0;


// 화면 지우기 (OS 호환성 고려)
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// 사용자 입력을 기다리는 함수
void pauseExecution() {
    printf("\n계속하려면 Enter 키를 누르세요...");
    getchar(); // 이전 엔터키를 소비
    getchar(); // 실제 엔터키 입력 대기
}

// 현재 날짜를 YYYY-MM-DD 형식으로 가져오는 함수
void getCurrentDate(char* dateStr) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(dateStr, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

// 두 날짜 문자열이 같은지 비교하는 함수
int isSameDate(const char* date1, const char* date2) {
    return strcmp(date1, date2) == 0;
}

// 개행 문자 제거 함수 (fgets 사용 시 유용)
void removeNewline(char* str) {
    str[strcspn(str, "\n")] = 0;
}


// --- 데이터 로드/저장 함수 ---

// 사용자 데이터 로드
void loadUsers() {
    FILE* fp = fopen(USERS_FILE, "r");
    if (fp == NULL) {
        printf("사용자 데이터 파일을 찾을 수 없습니다. 새로운 파일을 생성합니다.\n");
        return;
    }
    numUsers = 0;
    while (numUsers < MAX_USERS && fscanf(fp, "%s %s %d %s %s %d\n",
                                          users[numUsers].id,
                                          users[numUsers].password,
                                          &users[numUsers].coins,
                                          users[numUsers].lastTruthDate,
                                          users[numUsers].lastDareDate,
                                          &users[numUsers].dareAttemptsToday) == 6) {
        numUsers++;
    }
    fclose(fp);
    printf("사용자 데이터 로드 완료: %d명\n", numUsers);
}

// 사용자 데이터 저장
void saveUsers() {
    FILE* fp = fopen(USERS_FILE, "w");
    if (fp == NULL) {
        printf("사용자 데이터 파일을 저장할 수 없습니다.\n");
        return;
    }
    for (int i = 0; i < numUsers; i++) {
        fprintf(fp, "%s %s %d %s %s %d\n",
                users[i].id,
                users[i].password,
                users[i].coins,
                users[i].lastTruthDate,
                users[i].lastDareDate,
                users[i].dareAttemptsToday);
    }
    fclose(fp);
    printf("사용자 데이터 저장 완료.\n");
}

// Truth 질문 로드
void loadTruthQuestions() {
    FILE* fp = fopen(TRUTH_QUESTIONS_FILE, "r");
    if (fp == NULL) {
        printf("Truth 질문 파일을 찾을 수 없습니다. 기본 질문을 사용합니다.\n");
        // 기본 질문 설정 (파일이 없을 경우)
        truthQuestions[0] = (TruthQuestion){1, "오늘 가장 감사했던 일은 무엇인가요?", 0};
        truthQuestions[1] = (TruthQuestion){2, "최근 자신을 성장시켰다고 생각하는 경험은 무엇인가요?", 0};
        truthQuestions[2] = (TruthQuestion){3, "오늘 하루 느꼈던 감정을 색깔로 표현한다면 어떤 색깔인가요?", 0};
        numTruthQuestions = 3;
        return;
    }
    numTruthQuestions = 0;
    char line[MAX_QUESTION_LEN + 10]; // ID + Question
    while (numTruthQuestions < MAX_QUESTIONS && fgets(line, sizeof(line), fp) != NULL) {
        // fscanf 대신 fgets로 한 줄을 읽고 sscanf로 파싱
        sscanf(line, "%d %[^\n]", &truthQuestions[numTruthQuestions].id, truthQuestions[numTruthQuestions].question);
        truthQuestions[numTruthQuestions].used = 0; // 초기화 시 사용되지 않음으로 설정
        numTruthQuestions++;
    }
    fclose(fp);
    printf("Truth 질문 로드 완료: %d개\n", numTruthQuestions);
}

// Dare 도전 로드
void loadDareChallenges() {
    FILE* fp = fopen(DARE_CHALLENGES_FILE, "r");
    if (fp == NULL) {
        printf("Dare 도전 파일을 찾을 수 없습니다. 기본 도전을 사용합니다.\n");
        // 기본 도전 설정 (파일이 없을 경우)
        dareChallenges[0] = (DareChallenge){101, "신체", "팔굽혀펴기 10개 하기"};
        dareChallenges[1] = (DareChallenge){102, "학습", "새로운 단어 5개 외우기"};
        dareChallenges[2] = (DareChallenge){103, "정서", "거울 보고 자신에게 칭찬 한마디 하기"};
        numDareChallenges = 3;
        return;
    }
    numDareChallenges = 0;
    char line[MAX_QUESTION_LEN + MAX_CATEGORY_LEN + 20]; // ID + Category + Challenge
    while (numDareChallenges < MAX_DARES && fgets(line, sizeof(line), fp) != NULL) {
        sscanf(line, "%d %s %[^\n]", &dareChallenges[numDareChallenges].id,
               dareChallenges[numDareChallenges].category, dareChallenges[numDareChallenges].challenge);
        numDareChallenges++;
    }
    fclose(fp);
    printf("Dare 도전 로드 완료: %d개\n", numDareChallenges);
}


// 사용자 기록 로드
void loadUserRecords() {
    FILE* fp = fopen(RECORDS_FILE, "r");
    if (fp == NULL) {
        printf("기록 데이터 파일을 찾을 수 없습니다. 새로운 파일을 생성합니다.\n");
        return;
    }
    numRecords = 0;
    while (numRecords < MAX_RECORDS && fscanf(fp, "%s %s %d %d %d %[^\n]\n",
                                              userRecords[numRecords].userId,
                                              userRecords[numRecords].date,
                                              &userRecords[numRecords].type,
                                              &userRecords[numRecords].contentId,
                                              &userRecords[numRecords].coinsEarned,
                                              userRecords[numRecords].response) == 6) {
        // 개행 문자가 남아있을 수 있으므로 추가 처리
        removeNewline(userRecords[numRecords].response);
        numRecords++;
    }
    fclose(fp);
    printf("기록 데이터 로드 완료: %d개\n", numRecords);
}

// 사용자 기록 저장
void saveUserRecords() {
    FILE* fp = fopen(RECORDS_FILE, "w");
    if (fp == NULL) {
        printf("기록 데이터 파일을 저장할 수 없습니다.\n");
        return;
    }
    for (int i = 0; i < numRecords; i++) {
        fprintf(fp, "%s %s %d %d %d %s\n",
                userRecords[i].userId,
                userRecords[i].date,
                userRecords[i].type,
                userRecords[i].contentId,
                userRecords[i].coinsEarned,
                userRecords[i].response);
    }
    fclose(fp);
    printf("기록 데이터 저장 완료.\n");
}


// --- 로그인 및 사용자 관리 함수 ---

// 로그인 처리 또는 회원가입
int handleLogin() {
    char inputId[MAX_ID_LEN];
    char inputPw[MAX_PW_LEN];
    int userIdx = -1;

    while (userIdx == -1) {
        clearScreen();
        printf("=======================\n");
        printf("     Truth or Dare     \n");
        printf("=======================\n");
        printf("1. 로그인\n");
        printf("2. 회원가입\n");
        printf("0. 종료\n");
        printf("선택: ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            printf("잘못된 입력입니다. 다시 시도하세요.\n");
            while (getchar() != '\n'); // 입력 버퍼 비우기
            continue;
        }
        while (getchar() != '\n'); // 입력 버퍼 비우기

        if (choice == 0) return 0; // 프로그램 종료

        printf("ID를 입력하세요: ");
        fgets(inputId, sizeof(inputId), stdin);
        removeNewline(inputId);

        printf("비밀번호를 입력하세요: ");
        fgets(inputPw, sizeof(inputPw), stdin);
        removeNewline(inputPw);

        if (choice == 1) { // 로그인
            for (int i = 0; i < numUsers; i++) {
                if (strcmp(users[i].id, inputId) == 0 && strcmp(users[i].password, inputPw) == 0) {
                    userIdx = i;
                    break;
                }
            }
            if (userIdx != -1) {
                printf("로그인 성공!\n");
                currentUser = users[userIdx]; // 전역 currentUser 업데이트
            } else {
                printf("ID 또는 비밀번호가 일치하지 않습니다. 다시 시도하세요.\n");
                pauseExecution();
            }
        } else if (choice == 2) { // 회원가입
            if (numUsers >= MAX_USERS) {
                printf("더 이상 사용자를 추가할 수 없습니다.\n");
                pauseExecution();
                continue;
            }
            // ID 중복 확인
            int idExists = 0;
            for (int i = 0; i < numUsers; i++) {
                if (strcmp(users[i].id, inputId) == 0) {
                    idExists = 1;
                    break;
                }
            }
            if (idExists) {
                printf("이미 존재하는 ID입니다. 다른 ID를 사용하세요.\n");
                pauseExecution();
                continue;
            }

            strcpy(users[numUsers].id, inputId);
            strcpy(users[numUsers].password, inputPw);
            users[numUsers].coins = 0;
            strcpy(users[numUsers].lastTruthDate, "none"); // 초기값
            strcpy(users[numUsers].lastDareDate, "none");   // 초기값
            users[numUsers].dareAttemptsToday = 0;
            numUsers++;
            saveUsers(); // 사용자 추가 후 저장
            printf("회원가입 성공! 로그인해주세요.\n");
            pauseExecution();
        } else {
            printf("잘못된 선택입니다.\n");
            pauseExecution();
        }
    }
    return 1; // 로그인 성공
}

// 사용자 일일 상태 초기화
void resetDailyStatus() {
    char currentDate[MAX_DATE_LEN];
    getCurrentDate(currentDate);

    // 현재 로그인된 사용자의 정보 업데이트
    // users 배열에서도 해당 사용자 정보를 찾아 업데이트해야 합니다.
    for (int i = 0; i < numUsers; i++) {
        if (strcmp(users[i].id, currentUser.id) == 0) {
            // Truth 초기화
            if (!isSameDate(currentUser.lastTruthDate, currentDate)) {
                strcpy(currentUser.lastTruthDate, "none"); // 오늘 Truth 아직 안 함
                strcpy(users[i].lastTruthDate, "none");
            }
            // Dare 초기화
            if (!isSameDate(currentUser.lastDareDate, currentDate)) {
                currentUser.dareAttemptsToday = 0;
                strcpy(currentUser.lastDareDate, currentDate); // 오늘 Dare 시작 날짜로 업데이트
                users[i].dareAttemptsToday = 0;
                strcpy(users[i].lastDareDate, currentDate);
            }
            // 전역 currentUser 정보도 업데이트했으니, users 배열의 해당 사용자 정보도 업데이트
            // (함수 호출 후 saveUsers()를 통해 파일에 저장)
            break;
        }
    }
}

// 현재 로그인된 사용자 정보 업데이트 (users 배열과 동기화)
void updateCurrentUserInUsersArray() {
    for (int i = 0; i < numUsers; i++) {
        if (strcmp(users[i].id, currentUser.id) == 0) {
            users[i] = currentUser;
            break;
        }
    }
}



// --- 메인 메뉴 및 선택 함수 ---

void displayMainMenu() {
    printf("=======================\n");
    printf("     Truth or Dare     \n");
    printf("=======================\n");
    printf("1. Truth (오늘의 질문)\n");
    printf("2. Dare (오늘의 도전)\n");
    printf("3. 기록 보기\n");
    printf("4. 코인 기록\n");
    printf("0. 종료\n");
    printf("-----------------------\n");
    printf("현재 코인: %d\n", currentUser.coins); // 우측 상단 코인 표시
    printf("선택: ");
}

int getMenuChoice() {
    int choice;
    if (scanf("%d", &choice) != 1) {
        printf("잘못된 입력입니다. 숫자를 입력해주세요.\n");
        choice = -1; // 유효하지 않은 선택
    }
    while (getchar() != '\n'); // 입력 버퍼 비우기
    return choice;
}




// --- Truth 기능 함수 ---

// 랜덤 Truth 질문 가져오기
TruthQuestion* getRandomTruthQuestion() {
    char currentDate[MAX_DATE_LEN];
    getCurrentDate(currentDate);

    if (isSameDate(currentUser.lastTruthDate, currentDate)) {
        printf("오늘은 이미 Truth 질문에 답하셨습니다. 내일 다시 시도해주세요!\n");
        return NULL;
    }

    // 모든 질문을 'used = 0'으로 초기화
    for (int i = 0; i < numTruthQuestions; i++) {
        truthQuestions[i].used = 0;
    }

    int availableQuestionsCount = 0;
    for (int i = 0; i < numTruthQuestions; i++) {
        // 이미 사용된 질문 (여기서는 Truth 질문이 매일 리셋되므로 이 부분은 세션 내 중복 방지용)
        if (truthQuestions[i].used == 0) {
            availableQuestionsCount++;
        }
    }

    if (availableQuestionsCount == 0) {
        printf("더 이상 보여줄 Truth 질문이 없습니다.\n");
        return NULL;
    }

    int randomIndex;
    TruthQuestion* selectedQuestion = NULL;
    do {
        randomIndex = rand() % numTruthQuestions;
        if (truthQuestions[randomIndex].used == 0) {
            selectedQuestion = &truthQuestions[randomIndex];
            selectedQuestion->used = 1; // 사용됨으로 표시
            break;
        }
    } while (1);

    return selectedQuestion;
}

// Truth 질문 및 답변 처리
void handleTruth() {
    clearScreen();
    TruthQuestion* currentQuestion = getRandomTruthQuestion();
    if (currentQuestion == NULL) {
        pauseExecution();
        return;
    }

    printf("=======================\n");
    printf("       오늘의 Truth      \n");
    printf("=======================\n");
    printf("질문: %s\n", currentQuestion->question);
    printf("답변 (최대 %d자): ", MAX_ANSWER_LEN - 1);

    char answer[MAX_ANSWER_LEN];
    fgets(answer, sizeof(answer), stdin);
    removeNewline(answer);

    // 기록 저장
    strcpy(userRecords[numRecords].userId, currentUser.id);
    getCurrentDate(userRecords[numRecords].date);
    userRecords[numRecords].type = 0; // Truth
    userRecords[numRecords].contentId = currentQuestion->id;
    strcpy(userRecords[numRecords].response, answer);
    userRecords[numRecords].coinsEarned = 0;
    numRecords++;
    saveUserRecords(); // 기록 파일에 즉시 저장

    // 사용자의 마지막 Truth 날짜 업데이트
    getCurrentDate(currentUser.lastTruthDate);
    updateCurrentUserInUsersArray(); // users 배열에도 업데이트
    saveUsers(); // 사용자 정보 저장

    printf("\n답변이 저장되었습니다. 언제든지 '기록 보기'에서 확인할 수 있습니다.\n");
    printf("추가 버튼을 누르면 첫 화면으로 돌아갑니다.\n"); // '추가' 버튼은 그냥 엔터로 대체
    // 여기서 '완료'와 '추가' 버튼은 실제 GUI가 아니므로, 저장은 '완료' 개념으로 하고
    // '추가'는 그냥 메인 메뉴로 돌아가는 동작으로 간주합니다.
}




// --- Dare 기능 함수 ---

// 랜덤 Dare 도전 가져오기
DareChallenge* getRandomDareChallenge(const char* category) {
    DareChallenge* selectedDare = NULL; // 선택된 Dare의 포인터
    int availableIndices[MAX_DARES]; // 해당 카테고리에 맞는 도전들의 인덱스를 저장
    int availableCount = 0;

    // 해당 카테고리에 맞는 도전들의 인덱스를 수집
    for (int i = 0; i < numDareChallenges; i++) {
        if (strcmp(dareChallenges[i].category, category) == 0) {
            availableIndices[availableCount++] = i;
        }
    }

    if (availableCount == 0) {
        printf("선택하신 카테고리에 도전 과제가 없습니다.\n");
        return NULL;
    }

    // 랜덤으로 하나 선택
    int randomIndexInAvailable = rand() % availableCount;
    int actualIndex = availableIndices[randomIndexInAvailable]; // 실제 dareChallenges 배열에서의 인덱스

    selectedDare = &dareChallenges[actualIndex]; // 전역 배열의 주소를 반환

    return selectedDare;
}

// Dare 도전 처리
void handleDare() {
    char currentDate[MAX_DATE_LEN];
    getCurrentDate(currentDate);

    // Dare 시도 횟수 초기화 및 업데이트 (날짜가 바뀌면)
    if (!isSameDate(currentUser.lastDareDate, currentDate)) {
        currentUser.dareAttemptsToday = 0;
        strcpy(currentUser.lastDareDate, currentDate);
        updateCurrentUserInUsersArray();
        saveUsers();
    }

    if (currentUser.dareAttemptsToday >= MAX_DARE_ATTEMPTS_PER_DAY) {
        clearScreen();
        printf("=======================\n");
        printf("     Dare 도전 완료!     \n");
        printf("=======================\n");
        printf("오늘은 더 이상 Dare 도전을 할 수 없습니다.\n");
        printf("5회 도전을 모두 완료하셨습니다. 정말 대단해요!\n");
        pauseExecution();
        return;
    }

    clearScreen();
    printf("=======================\n");
    printf("     Dare 카테고리     \n");
    printf("=======================\n");
    printf("1. 신체\n");
    printf("2. 학습\n");
    printf("3. 정서\n");
    printf("0. 뒤로가기\n");
    printf("-----------------------\n");
    printf("오늘 남은 도전 횟수: %d회\n", MAX_DARE_ATTEMPTS_PER_DAY - currentUser.dareAttemptsToday);
    printf("선택: ");

    int categoryChoice;
    if (scanf("%d", &categoryChoice) != 1) {
        printf("잘못된 입력입니다. 숫자를 입력해주세요.\n");
        while (getchar() != '\n');
        pauseExecution();
        return;
    }
    while (getchar() != '\n');

    if (categoryChoice == 0) return; // 뒤로가기

    char selectedCategory[MAX_CATEGORY_LEN];
    switch (categoryChoice) {
        case 1: strcpy(selectedCategory, "신체"); break;
        case 2: strcpy(selectedCategory, "학습"); break;
        case 3: strcpy(selectedCategory, "정서"); break;
        default:
            printf("유효하지 않은 카테고리입니다.\n");
            pauseExecution();
            return;
    }

    DareChallenge* currentDare = getRandomDareChallenge(selectedCategory);
    if (currentDare == NULL) {
        pauseExecution();
        return;
    }

    clearScreen();
    printf("=======================\n");
    printf("       오늘의 Dare       \n");
    printf("=======================\n");
    printf("카테고리: %s\n", currentDare->category);
    printf("도전: %s\n", currentDare->challenge);
    printf("\n1. 도전 완료 (Complete)\n");
    printf("2. 도전 실패 (Fail)\n");
    printf("선택: ");

    int dareResultChoice;
    if (scanf("%d", &dareResultChoice) != 1) {
        printf("잘못된 입력입니다. 숫자를 입력해주세요.\n");
        while (getchar() != '\n');
        pauseExecution();
        return;
    }
    while (getchar() != '\n');

    currentUser.dareAttemptsToday++; // 시도 횟수 증가

    int coinsEarned = 0;
    char responseResult[MAX_ANSWER_LEN];

    if (dareResultChoice == 1) { // Complete
        currentUser.coins += 10; // 예시: 10코인 지급
        coinsEarned = 10;
        strcpy(responseResult, "Complete");
        printf("\n축하합니다! 코인 10개를 획득했습니다!\n");
    } else if (dareResultChoice == 2) { // Fail
        strcpy(responseResult, "Fail");
        printf("\n아쉽지만 다음 기회에! 코인은 변동 없습니다.\n");
    } else {
        printf("잘못된 선택입니다. 도전 결과가 기록되지 않습니다.\n");
        strcpy(responseResult, "Invalid Choice");
    }

    // 기록 저장
    strcpy(userRecords[numRecords].userId, currentUser.id);
    getCurrentDate(userRecords[numRecords].date);
    userRecords[numRecords].type = 1; // Dare
    userRecords[numRecords].contentId = currentDare->id;
    strcpy(userRecords[numRecords].response, responseResult);
    userRecords[numRecords].coinsEarned = coinsEarned;
    numRecords++;
    saveUserRecords(); // 기록 파일에 즉시 저장

    updateCurrentUserInUsersArray(); // users 배열에도 업데이트
    saveUsers(); // 사용자 정보 저장

    if (currentUser.dareAttemptsToday < MAX_DARE_ATTEMPTS_PER_DAY) {
        printf("\n다음 도전을 선택할 수 있습니다.\n");
        pauseExecution();
        handleDare(); // 다음 도전을 위해 재귀 호출 또는 루프
    } else {
        clearScreen();
        printf("=======================\n");
        printf("     Dare 도전 완료!     \n");
        printf("=======================\n");
        printf("오늘의 Dare 도전을 모두 완료하셨습니다. 정말 대단해요!\n");
        pauseExecution();
    }
}





// --- 기록 보기 함수 ---

void viewRecords() {
    clearScreen();
    printf("=======================\n");
    printf("       나의 기록        \n");
    printf("=======================\n");

    if (numRecords == 0) {
        printf("아직 기록이 없습니다.\n");
        pauseExecution();
        return;
    }

    // 사용자 기록 중 현재 사용자의 기록만 필터링 및 정렬 (간단한 버블 정렬 예시)
    UserRecord userSpecificRecords[MAX_RECORDS];
    int userRecordCount = 0;
    for (int i = 0; i < numRecords; i++) {
        if (strcmp(userRecords[i].userId, currentUser.id) == 0) {
            userSpecificRecords[userRecordCount++] = userRecords[i];
        }
    }

    if (userRecordCount == 0) {
        printf("아직 기록이 없습니다.\n");
        pauseExecution();
        return;
    }

    // 날짜순 정렬 (버블 정렬)
    for (int i = 0; i < userRecordCount - 1; i++) {
        for (int j = 0; j < userRecordCount - 1 - i; j++) {
            if (strcmp(userSpecificRecords[j].date, userSpecificRecords[j+1].date) > 0) {
                UserRecord temp = userSpecificRecords[j];
                userSpecificRecords[j] = userSpecificRecords[j+1];
                userSpecificRecords[j+1] = temp;
            }
        }
    }

    for (int i = 0; i < userRecordCount; i++) {
        printf("\n날짜: %s\n", userSpecificRecords[i].date);
        if (userSpecificRecords[i].type == 0) { // Truth 기록
            printf("종류: Truth\n");
            // 질문 내용 찾기
            char qText[MAX_QUESTION_LEN] = "알 수 없는 질문";
            for (int j = 0; j < numTruthQuestions; j++) {
                if (truthQuestions[j].id == userSpecificRecords[i].contentId) {
                    strcpy(qText, truthQuestions[j].question);
                    break;
                }
            }
            printf("질문: %s\n", qText);
            printf("답변: %s\n", userSpecificRecords[i].response);
        } else { // Dare 기록
            printf("종류: Dare\n");
            // 도전 내용 찾기
            char dText[MAX_QUESTION_LEN] = "알 수 없는 도전";
            char dCategory[MAX_CATEGORY_LEN] = "N/A";
            for (int j = 0; j < numDareChallenges; j++) {
                if (dareChallenges[j].id == userSpecificRecords[i].contentId) {
                    strcpy(dText, dareChallenges[j].challenge);
                    strcpy(dCategory, dareChallenges[j].category);
                    break;
                }
            }
            printf("카테고리: %s\n", dCategory);
            printf("도전: %s\n", dText);
            printf("결과: %s (획득 코인: %d)\n", userSpecificRecords[i].response, userSpecificRecords[i].coinsEarned);
        }
        printf("-----------------------\n");
    }
    pauseExecution();
}




// --- 코인 기록 (랭킹) 함수 ---

void viewCoinRanking() {
    clearScreen();
    printf("=======================\n");
    printf("       코인 랭킹        \n");
    printf("=======================\n");

    if (numUsers == 0) {
        printf("등록된 사용자가 없습니다.\n");
        pauseExecution();
        return;
    }

    // 사용자 배열을 코인 기준으로 내림차순 정렬 (버블 정렬)
    User sortedUsers[MAX_USERS];
    memcpy(sortedUsers, users, sizeof(User) * numUsers); // 원본 배열 복사

    for (int i = 0; i < numUsers - 1; i++) {
        for (int j = 0; j < numUsers - 1 - i; j++) {
            if (sortedUsers[j].coins < sortedUsers[j+1].coins) {
                User temp = sortedUsers[j];
                sortedUsers[j] = sortedUsers[j+1];
                sortedUsers[j+1] = temp;
            }
        }
    }

    printf("--- TOP 3 ---\n");
    for (int i = 0; i < (numUsers > 3 ? 3 : numUsers); i++) {
        printf("%d위: %s - %d 코인\n", i + 1, sortedUsers[i].id, sortedUsers[i].coins);
    }

    printf("\n--- 나의 순위 ---\n");
    int myRank = -1;
    for (int i = 0; i < numUsers; i++) {
        if (strcmp(sortedUsers[i].id, currentUser.id) == 0) {
            myRank = i + 1;
            break;
        }
    }
    printf("%d위: %s - %d 코인\n", myRank, currentUser.id, currentUser.coins);

    pauseExecution();
}






// --- Main 함수 ---

int main() {
    srand(time(NULL)); // 난수 시드 초기화

    // 1. 데이터 로드 (시작 시)
    loadUsers();
    loadTruthQuestions();
    loadDareChallenges();
    loadUserRecords();

    // 2. 로그인 및 사용자 초기화
    if (!handleLogin()) {
        printf("로그인 과정이 취소되었습니다. 프로그램을 종료합니다.\n");
        return 0; // 로그인 실패 시 종료
    }

    // 3. 일일 상태 초기화 (날짜 변경 시)
    resetDailyStatus(); // 로그인 성공 후 현재 사용자 데이터 기반으로 초기화

    int choice;
    do {
        clearScreen();
        displayMainMenu();
        choice = getMenuChoice();

        switch (choice) {
            case 1: // Truth
                handleTruth();
                break;
            case 2: // Dare
                handleDare();
                break;
            case 3: // 기록 보기
                viewRecords();
                break;
            case 4: // 코인 기록
                viewCoinRanking();
                break;
            case 0: // 종료
                printf("프로그램을 종료합니다. 안녕히 계세요!\n");
                break;
            default:
                printf("잘못된 선택입니다. 다시 시도해주세요.\n");
                pauseExecution();
                break;
        }
    } while (choice != 0);

    // 4. 데이터 저장 (종료 시)
    saveUsers();
    saveUserRecords();

    return 0;
}




