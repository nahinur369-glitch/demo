#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L
#endif

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

#define ROWS 10
#define COLS 20
#define ANIMATION_DELAY_MS 45

#define C_RESET   "\x1b[0m"
#define C_WALL    "\x1b[47m"
#define C_START   "\x1b[1;32m"
#define C_END     "\x1b[1;31m"
#define C_VISIT   "\x1b[1;36m"
#define C_PATH    "\x1b[1;33m"

static const int dRow[4] = {-1, 1, 0, 0};
static const int dCol[4] = {0, 0, -1, 1};

typedef struct {
    int r;
    int c;
} Point;

typedef struct {
    bool found;
    int nodesVisited;
    int pathLength;
    int peakStructure;   /* DFS: max recursion depth, BFS: max queue size */
    double cpuTimeMs;
} Metrics;

static const char default_maze[ROWS][COLS + 1] = {
    "S #       #        #",
    "  #  ###  #  ####  #",
    "     #    #  #     #",
    "###  #  ###  #  ####",
    "  #  #       #     #",
    "  #  #####   ####  #",
    "  #          #     #",
    "  #######    #  ####",
    "        #    #     E",
    "#####   #    #######"
};

static char maze[ROWS][COLS + 1];
static bool visited[ROWS][COLS];
static Point parentCell[ROWS][COLS];

static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

static void enableAnsi(void) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

static void clearScreen(void) {
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
}

static void resetMaze(void) {
    memcpy(maze, default_maze, sizeof(maze));
    memset(visited, 0, sizeof(visited));

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            parentCell[r][c] = (Point){-1, -1};
        }
    }
}

static Point findCell(char target) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (maze[r][c] == target) {
                return (Point){r, c};
            }
        }
    }
    return (Point){-1, -1};
}

static bool samePoint(Point a, Point b) {
    return a.r == b.r && a.c == b.c;
}

static bool isInside(int r, int c) {
    return r >= 0 && r < ROWS && c >= 0 && c < COLS;
}

static bool isValid(int r, int c) {
    return isInside(r, c) && maze[r][c] != '#' && !visited[r][c];
}

static void printMaze(const char *algorithm, int nodesVisited) {
    printf("\x1b[H");
    printf("=============================================\n");
    printf("           BFS & DFS MAZE ANALYSIS           \n");
    printf("=============================================\n");
    printf("Algorithm: %-8s   Explored nodes: %-4d\n\n", algorithm, nodesVisited);

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            switch (maze[r][c]) {
                case '#': printf(C_WALL "  " C_RESET); break;
                case 'S': printf(C_START "S " C_RESET); break;
                case 'E': printf(C_END "E " C_RESET); break;
                case '.': printf(C_VISIT ". " C_RESET); break;
                case '*': printf(C_PATH "* " C_RESET); break;
                default:  printf("  "); break;
            }
        }
        putchar('\n');
    }

    printf("\nLegend: S=Start  E=End  .=Visited  *=Final path\n");
    fflush(stdout);
}

static int reconstructPath(Point start, Point end) {
    if (samePoint(start, end)) {
        return 0;
    }

    int length = 0;
    Point curr = end;

    while (!samePoint(curr, start)) {
        Point p = parentCell[curr.r][curr.c];
        if (p.r == -1 || p.c == -1) {
            return -1;
        }

        length++;
        curr = p;

        if (!samePoint(curr, start) && maze[curr.r][curr.c] != 'E') {
            maze[curr.r][curr.c] = '*';
        }
    }

    return length;
}

static bool dfsVisit(int r, int c, Point end, Metrics *m, int depth, bool animate) {
    visited[r][c] = true;
    m->nodesVisited++;

    if (depth > m->peakStructure) {
        m->peakStructure = depth;
    }

    if (r == end.r && c == end.c) {
        return true;
    }

    if (maze[r][c] != 'S' && maze[r][c] != 'E') {
        maze[r][c] = '.';
    }

    if (animate) {
        printMaze("DFS", m->nodesVisited);
        sleep_ms(ANIMATION_DELAY_MS);
    }

    for (int i = 0; i < 4; i++) {
        int nr = r + dRow[i];
        int nc = c + dCol[i];

        if (isValid(nr, nc)) {
            parentCell[nr][nc] = (Point){r, c};
            if (dfsVisit(nr, nc, end, m, depth + 1, animate)) {
                return true;
            }
        }
    }

    return false;
}

static Metrics runDFS(bool animate) {
    Metrics m = {0};
    Point start = findCell('S');
    Point end = findCell('E');

    if (start.r == -1 || end.r == -1) {
        return m;
    }

    clock_t begin = clock();
    m.found = dfsVisit(start.r, start.c, end, &m, 1, animate);
    clock_t finish = clock();
    m.cpuTimeMs = 1000.0 * (double)(finish - begin) / CLOCKS_PER_SEC;

    if (m.found) {
        m.pathLength = reconstructPath(start, end);
    }

    if (animate) {
        printMaze("DFS", m.nodesVisited);
    }

    return m;
}

static Metrics runBFS(bool animate) {
    Metrics m = {0};
    Point start = findCell('S');
    Point end = findCell('E');

    if (start.r == -1 || end.r == -1) {
        return m;
    }

    Point queue[ROWS * COLS];
    int front = 0;
    int rear = 0;

    queue[rear++] = start;
    visited[start.r][start.c] = true;
    m.peakStructure = 1;

    clock_t begin = clock();

    while (front < rear) {
        Point curr = queue[front++];
        m.nodesVisited++;

        if (samePoint(curr, end)) {
            m.found = true;
            break;
        }

        if (maze[curr.r][curr.c] != 'S' && maze[curr.r][curr.c] != 'E') {
            maze[curr.r][curr.c] = '.';
        }

        if (animate) {
            printMaze("BFS", m.nodesVisited);
            sleep_ms(ANIMATION_DELAY_MS);
        }

        for (int i = 0; i < 4; i++) {
            int nr = curr.r + dRow[i];
            int nc = curr.c + dCol[i];

            if (isValid(nr, nc)) {
                visited[nr][nc] = true;  /* mark when enqueued */
                parentCell[nr][nc] = curr;
                queue[rear++] = (Point){nr, nc};

                int queueSize = rear - front;
                if (queueSize > m.peakStructure) {
                    m.peakStructure = queueSize;
                }
            }
        }
    }

    clock_t finish = clock();
    m.cpuTimeMs = 1000.0 * (double)(finish - begin) / CLOCKS_PER_SEC;

    if (m.found) {
        m.pathLength = reconstructPath(start, end);
    }

    if (animate) {
        printMaze("BFS", m.nodesVisited);
    }

    return m;
}

static void printSingleResult(const char *name, Metrics m) {
    printf("\n%s RESULT\n", name);
    printf("---------------------------------------------\n");
    printf("Path found       : %s\n", m.found ? "YES" : "NO");
    printf("Explored nodes   : %d\n", m.nodesVisited);
    printf("Path length      : %d moves\n", m.found ? m.pathLength : -1);
    printf("CPU time         : %.3f ms\n", m.cpuTimeMs);

    if (name[0] == 'D') {
        printf("Max DFS depth    : %d\n", m.peakStructure);
    } else {
        printf("Max BFS queue    : %d\n", m.peakStructure);
    }
}

static void runComparison(void) {
    resetMaze();
    Metrics dfs = runDFS(false);

    resetMaze();
    Metrics bfs = runBFS(false);

    clearScreen();
    printf("==============================================================\n");
    printf("                 BFS vs DFS COMPARISON                        \n");
    printf("             (benchmark without animation)                    \n");
    printf("==============================================================\n");
    printf("%-22s %-14s %-14s\n", "Metric", "DFS", "BFS");
    printf("--------------------------------------------------------------\n");
    printf("%-22s %-14s %-14s\n", "Path found", dfs.found ? "YES" : "NO", bfs.found ? "YES" : "NO");
    printf("%-22s %-14d %-14d\n", "Explored nodes", dfs.nodesVisited, bfs.nodesVisited);
    printf("%-22s %-14d %-14d\n", "Path length", dfs.found ? dfs.pathLength : -1, bfs.found ? bfs.pathLength : -1);
    printf("%-22s %-14.3f %-14.3f\n", "CPU time (ms)", dfs.cpuTimeMs, bfs.cpuTimeMs);
    printf("%-22s %-14d %-14d\n", "Peak structure", dfs.peakStructure, bfs.peakStructure);

    printf("\nAnalysis:\n");
    printf("- BFS guarantees a shortest path in this unweighted maze.\n");
    printf("- DFS can find a valid path, but it does not guarantee the shortest one.\n");
    printf("- Explored-node count depends on neighbor order and maze structure.\n");
    printf("- Peak structure means recursion depth for DFS and queue size for BFS.\n");
}

static void blockAllEntrancesToEnd(void) {
    Point end = findCell('E');
    if (end.r == -1) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        int nr = end.r + dRow[i];
        int nc = end.c + dCol[i];
        if (isInside(nr, nc) && maze[nr][nc] != '#' && maze[nr][nc] != 'S') {
            maze[nr][nc] = '#';
        }
    }
}

static int readChoice(void) {
    char line[64];
    char *endPtr;

    printf("Select an option: ");
    if (!fgets(line, sizeof(line), stdin)) {
        return 5;
    }

    long value = strtol(line, &endPtr, 10);
    if (endPtr == line) {
        return -1;
    }
    return (int)value;
}

static void pressEnter(void) {
    char line[8];
    printf("\nPress Enter to continue...");
    fgets(line, sizeof(line), stdin);
}

int main(void) {
    enableAnsi();

    while (1) {
        clearScreen();
        printf("=============================================\n");
        printf("        BFS & DFS MAZE ANALYSIS PROJECT      \n");
        printf("=============================================\n");
        printf("1. Run DFS visualization\n");
        printf("2. Run BFS visualization\n");
        printf("3. Compare BFS vs DFS\n");
        printf("4. Test unreachable target\n");
        printf("5. Exit\n\n");

        int choice = readChoice();

        switch (choice) {
            case 1: {
                resetMaze();
                clearScreen();
                Metrics m = runDFS(true);
                printSingleResult("DFS", m);
                pressEnter();
                break;
            }

            case 2: {
                resetMaze();
                clearScreen();
                Metrics m = runBFS(true);
                printSingleResult("BFS", m);
                pressEnter();
                break;
            }

            case 3:
                runComparison();
                pressEnter();
                break;

            case 4: {
                resetMaze();
                blockAllEntrancesToEnd();
                clearScreen();
                Metrics m = runBFS(true);

                if (!m.found) {
                    printf("\nEDGE CASE PASSED: Target is unreachable; no path exists.\n");
                } else {
                    printf("\nUnexpected: a path was found.\n");
                }

                printSingleResult("BFS", m);
                pressEnter();
                break;
            }

            case 5:
                clearScreen();
                printf("Program closed.\n");
                return 0;

            default:
                printf("\nInvalid option. Please enter a number from 1 to 5.\n");
                sleep_ms(900);
        }
    }
}
