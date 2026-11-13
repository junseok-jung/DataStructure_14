#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* 성능 측정 결과를 담는 구조체 */
typedef struct {
    long long comparisons;
    double time_ms;
    size_t memory_bytes;
} SortMetrics;

/*퀵 정렬 피벗 선택 방식 열거형 */
typedef enum {
    PIVOT_FIRST,
    PIVOT_RANDOM,
    PIVOT_MEDIAN3
} PivotType;

/*
 * 함수 호출 스택 프레임 및 보조 변수 크기 정의 (64비트 환경 기준)
 * - HEAP_AUX_BYTES: heap_sort 및 heapify 내부 지역/보조 변수 (i, root, child, temp 등)
 * - MERGE_FRAME_BYTES: merge_sort 재귀 호출 1회당 스택 프레임 크기
 * - QUICK_FRAME_BYTES: quick_sort 재귀 호출 1회당 스택 프레임 크기
 */
#define HEAP_AUX_BYTES    32
#define MERGE_FRAME_BYTES 48
#define QUICK_FRAME_BYTES 48

 /* 고해상도 현재 시간(ms) 반환 함수 */
double get_time_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

/* 두 정수 교환 매크로 */
static inline void swap_int(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

/* 오름차순 정렬 완료 여부 검증 함수 */
int is_sorted(const int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        if (arr[i] > arr[i + 1]) {
            return 0;
        }
    }
    return 1;
}

/* =========================================================================
 * [1] 힙 정렬 (Heap Sort) - 입력 배열 자체를 이용한 제자리(In-place) 최대 힙
 * ========================================================================= */
void sift_down(int arr[], int heap_size, int root, long long* comparisons) {
    int temp = arr[root];
    int parent = root;

    while (parent * 2 + 1 < heap_size) {
        int left = parent * 2 + 1;
        int right = left + 1;
        int largest_child = left;

        /* 오른쪽 자식이 존재하면 두 자식의 데이터 값을 비교 */
        if (right < heap_size) {
            (*comparisons)++;
            if (arr[right] > arr[left]) {
                largest_child = right;
            }
        }

        /* 부모(temp)와 더 큰 자식의 데이터 값을 비교 */
        (*comparisons)++;
        if (arr[largest_child] > temp) {
            arr[parent] = arr[largest_child];
            parent = largest_child;
        }
        else {
            break;
        }
    }
    arr[parent] = temp;
}

SortMetrics run_heap_sort(int arr[], int n) {
    SortMetrics m = { 0, 0.0, HEAP_AUX_BYTES };
    double start = get_time_ms();

    /* 1. 입력 배열을 최대 힙(Max Heap)으로 구성 */
    for (int i = n / 2 - 1; i >= 0; i--) {
        sift_down(arr, n, i, &m.comparisons);
    }

    /* 2. 루트의 최댓값을 마지막 원소와 교환 후 힙 크기 줄이며 힙 속성 복구 */
    for (int i = n - 1; i > 0; i--) {
        swap_int(&arr[0], &arr[i]);
        sift_down(arr, i, 0, &m.comparisons);
    }

    double end = get_time_ms();
    m.time_ms = end - start;
    return m;
}

/* =========================================================================
 * [2] 병합 정렬 (Merge Sort)
 * ========================================================================= */
void merge(int arr[], int temp[], int left, int mid, int right, long long* comparisons) {
    int i = left;
    int j = mid + 1;
    int k = left;

    while (i <= mid && j <= right) {
        (*comparisons)++; /* 두 부분 배열 원소 간 값 비교 */
        if (arr[i] <= arr[j]) {
            temp[k++] = arr[i++];
        }
        else {
            temp[k++] = arr[j++];
        }
    }

    while (i <= mid) {
        temp[k++] = arr[i++];
    }
    while (j <= right) {
        temp[k++] = arr[j++];
    }

    for (i = left; i <= right; i++) {
        arr[i] = temp[i];
    }
}

void merge_sort_rec(int arr[], int temp[], int left, int right,
    long long* comparisons, int depth, int* max_depth) {
    if (depth > *max_depth) {
        *max_depth = depth;
    }
    if (left >= right) {
        return;
    }

    int mid = left + (right - left) / 2;
    merge_sort_rec(arr, temp, left, mid, comparisons, depth + 1, max_depth);
    merge_sort_rec(arr, temp, mid + 1, right, comparisons, depth + 1, max_depth);
    merge(arr, temp, left, mid, right, comparisons);
}

SortMetrics run_merge_sort(int arr[], int n) {
    SortMetrics m = { 0, 0.0, 0 };
    int max_depth = 0;

    double start = get_time_ms();

    int* temp = (int*)malloc(sizeof(int) * n);
    if (!temp) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    merge_sort_rec(arr, temp, 0, n - 1, &m.comparisons, 1, &max_depth);
    free(temp);

    double end = get_time_ms();
    m.time_ms = end - start;
    /* 임시 배열 크기(n * sizeof(int)) + 최대 재귀 호출 스택 크기 */
    m.memory_bytes = (size_t)n * sizeof(int) + (size_t)max_depth * MERGE_FRAME_BYTES;
    return m;
}

/* =========================================================================
 * [3] 퀵 정렬 (Quick Sort - First / Random / Median-of-Three)
 * ========================================================================= */
int select_pivot(int arr[], int left, int right, PivotType type, long long* comparisons) {
    if (type == PIVOT_FIRST) {
        return left;
    }
    else if (type == PIVOT_RANDOM) {
        return left + rand() % (right - left + 1);
    }
    else {
        /* Median-of-Three: left, mid, right 세 원소의 중앙값 인덱스 선택 (값 비교 횟수 포함) */
        int mid = left + (right - left) / 2;

        (*comparisons)++;
        if (arr[left] > arr[mid]) {
            swap_int(&arr[left], &arr[mid]);
        }
        (*comparisons)++;
        if (arr[left] > arr[right]) {
            swap_int(&arr[left], &arr[right]);
        }
        (*comparisons)++;
        if (arr[mid] > arr[right]) {
            swap_int(&arr[mid], &arr[right]);
        }
        return mid;
    }
}

int partition(int arr[], int left, int right, PivotType type, long long* comparisons) {
    int pivot_idx = select_pivot(arr, left, right, type, comparisons);
    swap_int(&arr[left], &arr[pivot_idx]); /* 피벗을 첫 번째 위치로 이동 */

    int pivot = arr[left];
    int i = left;

    for (int j = left + 1; j <= right; j++) {
        (*comparisons)++; /* 피벗과 배열 원소 간 데이터 값 비교 */
        if (arr[j] < pivot) {
            i++;
            swap_int(&arr[i], &arr[j]);
        }
    }
    swap_int(&arr[left], &arr[i]);
    return i;
}

void quick_sort_rec(int arr[], int left, int right, PivotType type,
    long long* comparisons, int depth, int* max_depth) {
    if (depth > *max_depth) {
        *max_depth = depth;
    }
    if (left >= right) {
        return;
    }

    int pivot_pos = partition(arr, left, right, type, comparisons);
    quick_sort_rec(arr, left, pivot_pos - 1, type, comparisons, depth + 1, max_depth);
    quick_sort_rec(arr, pivot_pos + 1, right, type, comparisons, depth + 1, max_depth);
}

SortMetrics run_quick_sort(int arr[], int n, PivotType type) {
    SortMetrics m = { 0, 0.0, 0 };
    int max_depth = 0;

    double start = get_time_ms();
    quick_sort_rec(arr, 0, n - 1, type, &m.comparisons, 1, &max_depth);
    double end = get_time_ms();

    m.time_ms = end - start;
    m.memory_bytes = (size_t)max_depth * QUICK_FRAME_BYTES;
    return m;
}

/* =========================================================================
 * 메인 함수: 데이터 크기별 실험 및 결과 출력
 * ========================================================================= */
int main(void) {
    const int sizes[] = { 1000, 2000, 4000, 8000, 16000, 20000 };
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    srand(42); /* 재현 가능한 실험을 위한 난수 시드 설정 */

    for (int s = 0; s < num_sizes; s++) {
        int n = sizes[s];
        int* original = (int*)malloc(sizeof(int) * n);
        int* work = (int*)malloc(sizeof(int) * n);

        if (!original || !work) {
            fprintf(stderr, "Memory allocation failed\n");
            return EXIT_FAILURE;
        }

        /* 무작위 정수 생성 (중복 허용) */
        for (int i = 0; i < n; i++) {
            original[i] = rand() % (n * 2);
        }

        /* 1. Heap Sort */
        memcpy(work, original, sizeof(int) * n);
        SortMetrics m_heap = run_heap_sort(work, n);
        if (!is_sorted(work, n)) fprintf(stderr, "Heap Sort Failed!\n");

        /* 2. Merge Sort */
        memcpy(work, original, sizeof(int) * n);
        SortMetrics m_merge = run_merge_sort(work, n);
        if (!is_sorted(work, n)) fprintf(stderr, "Merge Sort Failed!\n");

        /* 3. Quick Sort (First Element) */
        memcpy(work, original, sizeof(int) * n);
        SortMetrics m_q_first = run_quick_sort(work, n, PIVOT_FIRST);
        if (!is_sorted(work, n)) fprintf(stderr, "Quick (First) Failed!\n");

        /* 4. Quick Sort (Random Pivot) */
        memcpy(work, original, sizeof(int) * n);
        SortMetrics m_q_rand = run_quick_sort(work, n, PIVOT_RANDOM);
        if (!is_sorted(work, n)) fprintf(stderr, "Quick (Random) Failed!\n");

        /* 5. Quick Sort (Median-of-Three) */
        memcpy(work, original, sizeof(int) * n);
        SortMetrics m_q_med3 = run_quick_sort(work, n, PIVOT_MEDIAN3);
        if (!is_sorted(work, n)) fprintf(stderr, "Quick (Median3) Failed!\n");

        /* 요구사항 출력 형식 */
        printf("Data Size : %d\n\n", n);
        printf("%-15s %-14s %-11s %s\n", "Algorithm", "Comparisons", "Time(ms)", "Memory(Bytes)");
        printf("--------------------------------------------------------\n");
        printf("%-15s %-14lld %-11.3f %zu\n", "Heap Sort", m_heap.comparisons, m_heap.time_ms, m_heap.memory_bytes);
        printf("%-15s %-14lld %-11.3f %zu\n", "Merge Sort", m_merge.comparisons, m_merge.time_ms, m_merge.memory_bytes);
        printf("%-15s %-14lld %-11.3f %zu\n", "Quick (First)", m_q_first.comparisons, m_q_first.time_ms, m_q_first.memory_bytes);
        printf("%-15s %-14lld %-11.3f %zu\n", "Quick (Random)", m_q_rand.comparisons, m_q_rand.time_ms, m_q_rand.memory_bytes);
        printf("%-15s %-14lld %-11.3f %zu\n\n", "Quick (Median3)", m_q_med3.comparisons, m_q_med3.time_ms, m_q_med3.memory_bytes);

        free(original);
        free(work);
    }

    return 0;
}