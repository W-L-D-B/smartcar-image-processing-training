/*
 * Grayscale camera-image laboratory, C99.
 *
 * The input is synthesized in this program. No camera SDK or private image
 * is required. All image processing is single-channel GRAY8.
 * Array parameters use C99-compatible qualifiers; input buffers are read-only
 * by convention unless their name identifies an output buffer.
 *
 * Build:
 *   gcc -std=c99 -O2 -Wall -Wextra image_lab.c -lm -o image_lab
 * Run:
 *   ./image_lab generated
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#define make_directory(path) _mkdir(path)
#else
#include <sys/types.h>
#define make_directory(path) mkdir(path, 0775)
#endif

#define IMG_W 80
#define IMG_H 48
#define OUT_W (IMG_W / 2)
#define OUT_H (IMG_H / 2)
#define PIXELS ((size_t)IMG_W * (size_t)IMG_H)
#define WHITE 255u
#define BLACK 0u
#if (IMG_W * IMG_H) > 65535
#error "This teaching queue stores pixel indices in uint16_t; increase the index type."
#endif

typedef uint8_t Gray[IMG_H][IMG_W];
typedef uint8_t Mask[IMG_H][IMG_W];
typedef uint8_t SmallGray[OUT_H][OUT_W];
typedef struct {
    int left;
    int right;
    int valid;
} EdgePair;

static uint8_t clamp_u8(int value)
{
    if (value < 0) return 0u;
    if (value > 255) return 255u;
    return (uint8_t)value;
}

static int save_pgm(const char *path, const uint8_t *pixels, int width, int height)
{
    FILE *file = fopen(path, "wb");
    int y;
    if (file == NULL) {
        fprintf(stderr, "cannot open output: %s\n", path);
        return 0;
    }
    fprintf(file, "P5\n%d %d\n255\n", width, height);
    for (y = 0; y < height; ++y) {
        if (fwrite(pixels + (size_t)y * (size_t)width, 1u,
                   (size_t)width, file) != (size_t)width) {
            fclose(file);
            fprintf(stderr, "cannot write output: %s\n", path);
            return 0;
        }
    }
    fclose(file);
    return 1;
}

static int save_gray(const char *dir, const char *name,
                     const uint8_t *pixels, int width, int height)
{
    char path[512];
    const int n = snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    return save_pgm(path, pixels, width, height);
}

static int ensure_directory(const char *path)
{
    if (make_directory(path) == 0 || errno == EEXIST) return 1;
    fprintf(stderr, "cannot create output directory: %s\n", path);
    return 0;
}

static void make_synthetic_frame(Gray image)
{
    int x, y;
    for (y = 0; y < IMG_H; ++y) {
        const int t = y * 255 / (IMG_H - 1);
        const int center = IMG_W / 2 + (y - 12) / 5 + ((y - 30) * (y - 30)) / 180;
        const int half_width = 8 + y / 9;
        const int left = center - half_width;
        const int right = center + half_width;
        for (x = 0; x < IMG_W; ++x) {
            int background = 38 + 22 * t / 255 + x / 8;
            int road = 174 + 38 * t / 255 - x / 30;
            int value = (x >= left && x <= right) ? road : background;

            /* A broad shadow and a small glare patch make thresholds nontrivial. */
            if (y >= 12 && y <= 28 && x >= 20 && x <= 47) value -= 47;
            if (y >= 5 && y <= 10 && x >= 51 && x <= 67) value = 244;

            /* Deterministic single-pixel noise; the same image is made each run. */
            if (((x * 17 + y * 31) % 173) == 0) value += 38;
            if (((x * 13 + y * 7) % 251) == 0) value -= 30;
            image[y][x] = clamp_u8(value);
        }
    }
}

static uint8_t otsu_threshold(Gray image)
{
    uint32_t histogram[256] = {0};
    uint64_t sum_all = 0;
    uint32_t count_background = 0;
    uint64_t sum_background = 0;
    double best_variance = -1.0;
    int best_threshold = 0;
    int i, y, x;

    for (y = 0; y < IMG_H; ++y) {
        for (x = 0; x < IMG_W; ++x) {
            const uint8_t value = image[y][x];
            histogram[value]++;
            sum_all += value;
        }
    }

    for (i = 0; i < 255; ++i) {
        double mean_background, mean_foreground, between;
        uint32_t count_foreground;
        count_background += histogram[i];
        sum_background += (uint64_t)i * histogram[i];
        count_foreground = (uint32_t)PIXELS - count_background;
        if (count_background == 0u || count_foreground == 0u) continue;
        mean_background = (double)sum_background / count_background;
        mean_foreground = (double)(sum_all - sum_background) / count_foreground;
        between = (double)count_background * count_foreground *
                  (mean_background - mean_foreground) *
                  (mean_background - mean_foreground);
        if (between > best_variance) {
            best_variance = between;
            best_threshold = i;
        }
    }
    return (uint8_t)best_threshold;
}

static uint8_t intermeans_threshold(const uint8_t *pixels, size_t count)
{
    uint8_t min_value = 255u, max_value = 0u;
    double threshold;
    size_t i;
    int iteration;

    if (pixels == NULL || count == 0u) return 0u;
    for (i = 0; i < count; ++i) {
        if (pixels[i] < min_value) min_value = pixels[i];
        if (pixels[i] > max_value) max_value = pixels[i];
    }
    threshold = ((double)min_value + (double)max_value) / 2.0;

    for (iteration = 0; iteration < 32; ++iteration) {
        uint64_t low_sum = 0, high_sum = 0;
        size_t low_count = 0, high_count = 0;
        double next;
        for (i = 0; i < count; ++i) {
            if (pixels[i] <= threshold) {
                low_sum += pixels[i];
                low_count++;
            } else {
                high_sum += pixels[i];
                high_count++;
            }
        }
        if (low_count == 0u || high_count == 0u) break;
        next = ((double)low_sum / (double)low_count +
                (double)high_sum / (double)high_count) / 2.0;
        if (fabs(next - threshold) < 0.5) {
            threshold = next;
            break;
        }
        threshold = next;
    }
    return clamp_u8((int)(threshold + 0.5));
}

static void threshold_fixed(Gray image, Mask mask, uint8_t threshold)
{
    int x, y;
    for (y = 0; y < IMG_H; ++y)
        for (x = 0; x < IMG_W; ++x)
            mask[y][x] = (image[y][x] >= threshold) ? WHITE : BLACK;
}

static void threshold_intermeans(Gray image, Mask mask)
{
    const uint8_t threshold = intermeans_threshold(&image[0][0], PIXELS);
    threshold_fixed(image, mask, threshold);
}

static void threshold_per_row(Gray image, Mask mask)
{
    int x, y;
    for (y = 0; y < IMG_H; ++y) {
        const uint8_t threshold =
            intermeans_threshold(image[y], (size_t)IMG_W);
        for (x = 0; x < IMG_W; ++x)
            mask[y][x] = (image[y][x] >= threshold) ? WHITE : BLACK;
    }
}

static void threshold_local_mean(Gray image, Mask mask, int radius, int offset)
{
    int x, y, xx, yy;
    for (y = 0; y < IMG_H; ++y) {
        for (x = 0; x < IMG_W; ++x) {
            uint32_t sum = 0;
            uint32_t count = 0;
            const int y0 = (y - radius < 0) ? 0 : y - radius;
            const int y1 = (y + radius >= IMG_H) ? IMG_H - 1 : y + radius;
            const int x0 = (x - radius < 0) ? 0 : x - radius;
            const int x1 = (x + radius >= IMG_W) ? IMG_W - 1 : x + radius;
            for (yy = y0; yy <= y1; ++yy) {
                for (xx = x0; xx <= x1; ++xx) {
                    sum += image[yy][xx];
                    count++;
                }
            }
            /* Bright road is the foreground; local threshold is mean + offset. */
            mask[y][x] = ((uint32_t)image[y][x] * count >= sum + (uint32_t)offset * count)
                         ? WHITE : BLACK;
        }
    }
}

static uint8_t difference_ratio_q7(uint8_t a, uint8_t b)
{
    const int32_t difference = (int32_t)a - (int32_t)b;
    const int32_t magnitude = (difference < 0) ? -difference : difference;
    const int32_t denominator = (int32_t)a + (int32_t)b + 1;
    return (uint8_t)((magnitude * 128) / denominator);
}

static void difference_ratio_mask(Gray image, Mask mask,
                                 int horizontal_gap, uint8_t threshold)
{
    int x, y;
    for (y = 0; y < IMG_H; ++y) {
        for (x = 0; x < IMG_W; ++x) {
            const int next_x = x + horizontal_gap;
            if (next_x >= IMG_W) {
                mask[y][x] = BLACK;
            } else {
                mask[y][x] = (difference_ratio_q7(image[y][x], image[y][next_x]) >= threshold)
                             ? WHITE : BLACK;
            }
        }
    }
}

static void downsample_2x(Gray image, SmallGray output)
{
    int x, y;
    for (y = 0; y < OUT_H; ++y)
        for (x = 0; x < OUT_W; ++x)
            output[y][x] = image[y * 2][x * 2];
}

static size_t pack_binary(Mask mask, uint8_t *packed, size_t capacity)
{
    size_t bit = 0, needed = (PIXELS + 7u) / 8u;
    int x, y;
    if (capacity < needed) return 0u;
    memset(packed, 0, needed);
    for (y = 0; y < IMG_H; ++y) {
        for (x = 0; x < IMG_W; ++x, ++bit) {
            if (mask[y][x] != 0u)
                packed[bit >> 3] |= (uint8_t)(1u << (7u - (bit & 7u)));
        }
    }
    return needed;
}

static void median_3x3(Gray input, Gray output)
{
    int x, y, yy, xx;
    memcpy(output, input, sizeof(Gray));
    for (y = 1; y < IMG_H - 1; ++y) {
        for (x = 1; x < IMG_W - 1; ++x) {
            uint8_t values[9];
            int n = 0, i, j;
            for (yy = y - 1; yy <= y + 1; ++yy)
                for (xx = x - 1; xx <= x + 1; ++xx)
                    values[n++] = input[yy][xx];
            for (i = 1; i < 9; ++i) {
                const uint8_t key = values[i];
                j = i - 1;
                while (j >= 0 && values[j] > key) {
                    values[j + 1] = values[j];
                    --j;
                }
                values[j + 1] = key;
            }
            output[y][x] = values[4];
        }
    }
}

static void erode_3x3(Mask input, Mask output)
{
    int x, y, dx, dy;
    memset(output, 0, sizeof(Mask));
    for (y = 1; y < IMG_H - 1; ++y) {
        for (x = 1; x < IMG_W - 1; ++x) {
            int keep_white = 1;
            for (dy = -1; dy <= 1 && keep_white; ++dy)
                for (dx = -1; dx <= 1; ++dx)
                    if (input[y + dy][x + dx] == BLACK) { keep_white = 0; break; }
            output[y][x] = keep_white ? WHITE : BLACK;
        }
    }
}

static void dilate_3x3(Mask input, Mask output)
{
    int x, y, dx, dy;
    memset(output, 0, sizeof(Mask));
    for (y = 1; y < IMG_H - 1; ++y) {
        for (x = 1; x < IMG_W - 1; ++x) {
            int set_white = 0;
            for (dy = -1; dy <= 1 && !set_white; ++dy)
                for (dx = -1; dx <= 1; ++dx)
                    if (input[y + dy][x + dx] != BLACK) { set_white = 1; break; }
            output[y][x] = set_white ? WHITE : BLACK;
        }
    }
}

static void morphology_open(Mask input, Mask output)
{
    Mask temporary;
    erode_3x3(input, temporary);
    dilate_3x3(temporary, output);
}

static void morphology_close(Mask input, Mask output)
{
    Mask temporary;
    dilate_3x3(input, temporary);
    erode_3x3(temporary, output);
}

static void sobel(Gray image, Gray gx_image, Gray gy_image, Gray magnitude)
{
    int x, y;
    memset(gx_image, 0, sizeof(Gray));
    memset(gy_image, 0, sizeof(Gray));
    memset(magnitude, 0, sizeof(Gray));
    for (y = 1; y < IMG_H - 1; ++y) {
        for (x = 1; x < IMG_W - 1; ++x) {
            const int gx =
                -(int)image[y - 1][x - 1] + (int)image[y - 1][x + 1]
                - 2 * (int)image[y][x - 1] + 2 * (int)image[y][x + 1]
                - (int)image[y + 1][x - 1] + (int)image[y + 1][x + 1];
            const int gy =
                -(int)image[y - 1][x - 1] - 2 * (int)image[y - 1][x] - (int)image[y - 1][x + 1]
                + (int)image[y + 1][x - 1] + 2 * (int)image[y + 1][x] + (int)image[y + 1][x + 1];
            const int abs_gx = (gx < 0) ? -gx : gx;
            const int abs_gy = (gy < 0) ? -gy : gy;
            gx_image[y][x] = clamp_u8(128 + gx / 4);
            gy_image[y][x] = clamp_u8(128 + gy / 4);
            magnitude[y][x] = clamp_u8((abs_gx + abs_gy) / 8);
        }
    }
}

static void sobel_threshold(Gray magnitude, Mask edges, uint8_t threshold)
{
    int x, y;
    for (y = 0; y < IMG_H; ++y)
        for (x = 0; x < IMG_W; ++x)
            edges[y][x] = (magnitude[y][x] >= threshold) ? WHITE : BLACK;
}

static int canny_simple(Gray image, Mask output, uint16_t low, uint16_t high)
{
    static const int gaussian[3][3] = {{1,2,1},{2,4,2},{1,2,1}};
    Gray blurred;
    int16_t gx_image[IMG_H][IMG_W] = {{0}};
    int16_t gy_image[IMG_H][IMG_W] = {{0}};
    uint16_t magnitude[IMG_H][IMG_W] = {{0}};
    uint16_t suppressed[IMG_H][IMG_W] = {{0}};
    uint16_t queue[IMG_H * IMG_W];
    size_t head = 0, tail = 0;
    int x, y, dx, dy;
    if (low > high) return 0;

    memcpy(blurred, image, sizeof(Gray));
    for (y = 1; y < IMG_H - 1; ++y) {
        for (x = 1; x < IMG_W - 1; ++x) {
            int sum = 0;
            for (dy = -1; dy <= 1; ++dy)
                for (dx = -1; dx <= 1; ++dx)
                    sum += gaussian[dy + 1][dx + 1] * image[y + dy][x + dx];
            blurred[y][x] = (uint8_t)((sum + 8) / 16);
        }
    }

    for (y = 1; y < IMG_H - 1; ++y) {
        for (x = 1; x < IMG_W - 1; ++x) {
            const int gx =
                -(int)blurred[y - 1][x - 1] + (int)blurred[y - 1][x + 1]
                - 2 * (int)blurred[y][x - 1] + 2 * (int)blurred[y][x + 1]
                - (int)blurred[y + 1][x - 1] + (int)blurred[y + 1][x + 1];
            const int gy =
                -(int)blurred[y - 1][x - 1] - 2 * (int)blurred[y - 1][x] - (int)blurred[y - 1][x + 1]
                + (int)blurred[y + 1][x - 1] + 2 * (int)blurred[y + 1][x] + (int)blurred[y + 1][x + 1];
            gx_image[y][x] = (int16_t)gx;
            gy_image[y][x] = (int16_t)gy;
            magnitude[y][x] = (uint16_t)(abs(gx) + abs(gy));
        }
    }

    /* Non-maximum suppression keeps only a local peak along gradient direction. */
    for (y = 1; y < IMG_H - 1; ++y) {
        for (x = 1; x < IMG_W - 1; ++x) {
            const int gx = gx_image[y][x], gy = gy_image[y][x];
            const int ax = abs(gx), ay = abs(gy);
            uint16_t before, after;
            if (2 * ay <= ax) {
                before = magnitude[y][x - 1]; after = magnitude[y][x + 1];
            } else if (2 * ax <= ay) {
                before = magnitude[y - 1][x]; after = magnitude[y + 1][x];
            } else if ((gx < 0) == (gy < 0)) {
                before = magnitude[y - 1][x - 1]; after = magnitude[y + 1][x + 1];
            } else {
                before = magnitude[y - 1][x + 1]; after = magnitude[y + 1][x - 1];
            }
            if (magnitude[y][x] >= before && magnitude[y][x] >= after)
                suppressed[y][x] = magnitude[y][x];
        }
    }

    memset(output, 0, sizeof(Mask));
    /* Strong edges seed an 8-connected queue; linked weak edges are retained. */
    for (y = 1; y < IMG_H - 1; ++y) {
        for (x = 1; x < IMG_W - 1; ++x) {
            if (suppressed[y][x] >= high) {
                output[y][x] = WHITE;
                queue[tail++] = (uint16_t)(y * IMG_W + x);
            }
        }
    }
    while (head < tail) {
        const uint16_t index = queue[head++];
        const int cy = index / IMG_W, cx = index % IMG_W;
        for (dy = -1; dy <= 1; ++dy) {
            for (dx = -1; dx <= 1; ++dx) {
                const int nx = cx + dx, ny = cy + dy;
                if ((dx != 0 || dy != 0) &&
                    nx > 0 && nx < IMG_W - 1 && ny > 0 && ny < IMG_H - 1 &&
                    output[ny][nx] == BLACK && suppressed[ny][nx] >= low) {
                    output[ny][nx] = WHITE;
                    queue[tail++] = (uint16_t)(ny * IMG_W + nx);
                }
            }
        }
    }
    return (int)tail;
}

static EdgePair scan_row_from_center(Mask binary, int y, int start_x)
{
    EdgePair result = {-1, -1, 0};
    int x, seed = start_x;
    if (seed < 1) seed = 1;
    if (seed >= IMG_W - 1) seed = IMG_W / 2;
    if (binary[y][seed] == BLACK) {
        int found = 0;
        for (x = 0; x < IMG_W && !found; ++x) {
            const int left = seed - x;
            const int right = seed + x;
            if (left >= 0 && binary[y][left] == WHITE) { seed = left; found = 1; }
            else if (right < IMG_W && binary[y][right] == WHITE) { seed = right; found = 1; }
        }
        if (!found) return result;
    }
    for (x = seed; x > 0; --x) {
        if (binary[y][x] == WHITE && binary[y][x - 1] == BLACK) {
            result.left = x;
            break;
        }
    }
    for (x = seed; x < IMG_W - 1; ++x) {
        if (binary[y][x] == WHITE && binary[y][x + 1] == BLACK) {
            result.right = x;
            break;
        }
    }
    result.valid = (result.left >= 0 && result.right > result.left);
    return result;
}

static void scan_each_row(Mask binary, int16_t left[IMG_H],
                          int16_t right[IMG_H], uint8_t valid[IMG_H])
{
    int y;
    for (y = IMG_H - 1; y >= 0; --y) {
        const EdgePair pair = scan_row_from_center(binary, y, IMG_W / 2);
        left[y] = (int16_t)pair.left;
        right[y] = (int16_t)pair.right;
        valid[y] = (uint8_t)pair.valid;
    }
}

static void scan_center_inherited(Mask binary, int16_t left[IMG_H],
                                  int16_t right[IMG_H], uint8_t valid[IMG_H])
{
    int y;
    int seed = IMG_W / 2;
    for (y = IMG_H - 1; y >= 0; --y) {
        const EdgePair pair = scan_row_from_center(binary, y, seed);
        left[y] = (int16_t)pair.left;
        right[y] = (int16_t)pair.right;
        valid[y] = (uint8_t)pair.valid;
        if (pair.valid) seed = (pair.left + pair.right) / 2;
    }
}

static int longest_white_seeds(Mask binary, int *left_seed, int *right_seed)
{
    int x, y, best_left_count = -1, best_right_count = -1;
    *left_seed = -1;
    *right_seed = -1;
    for (x = 1; x < IMG_W - 1; ++x) {
        int count = 0;
        for (y = IMG_H - 1; y >= 0 && binary[y][x] == WHITE; --y) ++count;
        if (count > best_left_count) {
            best_left_count = count;
            *left_seed = x;
        }
    }
    for (x = IMG_W - 2; x > 0; --x) {
        int count = 0;
        for (y = IMG_H - 1; y >= 0 && binary[y][x] == WHITE; --y) ++count;
        if (count > best_right_count) {
            best_right_count = count;
            *right_seed = x;
        }
    }
    return (best_left_count > 0 && best_right_count > 0)
           ? ((best_left_count < best_right_count) ? best_left_count : best_right_count)
           : 0;
}

static void scan_longest_column(Mask binary, int16_t left[IMG_H],
                               int16_t right[IMG_H], uint8_t valid[IMG_H],
                               int *left_seed_out, int *right_seed_out, int *stop_rows_out)
{
    int y, left_seed, right_seed;
    const int stop_rows = longest_white_seeds(binary, &left_seed, &right_seed);
    *left_seed_out = left_seed;
    *right_seed_out = right_seed;
    *stop_rows_out = stop_rows;
    for (y = 0; y < IMG_H; ++y) {
        EdgePair pair = {-1, -1, 0};
        if (stop_rows > 0 && y >= IMG_H - stop_rows)
            pair = scan_row_from_center(binary, y, (left_seed + right_seed) / 2);
        left[y] = (int16_t)pair.left;
        right[y] = (int16_t)pair.right;
        valid[y] = (uint8_t)pair.valid;
    }
}

static int trace_8_neighbor(Mask edges, int start_x, int start_y,
                            Gray trace, int max_steps)
{
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    uint8_t visited[IMG_H][IMG_W] = {{0}};
    int x = start_x, y = start_y, direction = 0, step;
    memset(trace, 0, sizeof(Gray));
    if (x < 0 || x >= IMG_W || y < 0 || y >= IMG_H || edges[y][x] == BLACK)
        return 0;

    for (step = 0; step < max_steps; ++step) {
        int k, found = 0;
        trace[y][x] = 255u;
        visited[y][x] = 1u;
        for (k = 0; k < 8; ++k) {
            const int d = (direction + 6 + k) & 7;
            const int nx = x + dx[d];
            const int ny = y + dy[d];
            if (nx >= 0 && nx < IMG_W && ny >= 0 && ny < IMG_H &&
                edges[ny][nx] != BLACK && !visited[ny][nx]) {
                x = nx;
                y = ny;
                direction = (d + 5) & 7;
                found = 1;
                break;
            }
        }
        if (!found) return step + 1;
    }
    return max_steps;
}

static int solve_8x8(double matrix[8][9], double answer[8])
{
    int col, row, pivot;
    for (col = 0; col < 8; ++col) {
        double largest = fabs(matrix[col][col]);
        pivot = col;
        for (row = col + 1; row < 8; ++row) {
            if (fabs(matrix[row][col]) > largest) {
                largest = fabs(matrix[row][col]);
                pivot = row;
            }
        }
        if (largest < 1e-10) return 0;
        if (pivot != col) {
            int k;
            for (k = col; k < 9; ++k) {
                const double swap = matrix[col][k];
                matrix[col][k] = matrix[pivot][k];
                matrix[pivot][k] = swap;
            }
        }
        {
            const double divisor = matrix[col][col];
            int k;
            for (k = col; k < 9; ++k) matrix[col][k] /= divisor;
        }
        for (row = 0; row < 8; ++row) {
            if (row != col) {
                const double factor = matrix[row][col];
                int k;
                for (k = col; k < 9; ++k)
                    matrix[row][k] -= factor * matrix[col][k];
            }
        }
    }
    for (row = 0; row < 8; ++row) answer[row] = matrix[row][8];
    return 1;
}

static int homography_from_quad(const double src[4][2], const double dst[4][2],
                                double h[9])
{
    double equations[8][9] = {{0}};
    double solution[8];
    int i;
    for (i = 0; i < 4; ++i) {
        const double x = src[i][0], y = src[i][1];
        const double u = dst[i][0], v = dst[i][1];
        const int r = 2 * i;
        equations[r][0] = x; equations[r][1] = y; equations[r][2] = 1.0;
        equations[r][6] = -u * x; equations[r][7] = -u * y;
        equations[r][8] = u;
        equations[r + 1][3] = x; equations[r + 1][4] = y; equations[r + 1][5] = 1.0;
        equations[r + 1][6] = -v * x; equations[r + 1][7] = -v * y;
        equations[r + 1][8] = v;
    }
    if (!solve_8x8(equations, solution)) return 0;
    for (i = 0; i < 8; ++i) h[i] = solution[i];
    h[8] = 1.0;
    return 1;
}

static int invert_3x3(const double m[9], double inv[9])
{
    const double determinant =
        m[0] * (m[4] * m[8] - m[5] * m[7]) -
        m[1] * (m[3] * m[8] - m[5] * m[6]) +
        m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (fabs(determinant) < 1e-10) return 0;
    inv[0] =  (m[4] * m[8] - m[5] * m[7]) / determinant;
    inv[1] = -(m[1] * m[8] - m[2] * m[7]) / determinant;
    inv[2] =  (m[1] * m[5] - m[2] * m[4]) / determinant;
    inv[3] = -(m[3] * m[8] - m[5] * m[6]) / determinant;
    inv[4] =  (m[0] * m[8] - m[2] * m[6]) / determinant;
    inv[5] = -(m[0] * m[5] - m[2] * m[3]) / determinant;
    inv[6] =  (m[3] * m[7] - m[4] * m[6]) / determinant;
    inv[7] = -(m[0] * m[7] - m[1] * m[6]) / determinant;
    inv[8] =  (m[0] * m[4] - m[1] * m[3]) / determinant;
    return 1;
}

static void warp_ipm(Gray input, Gray output, const double inverse_h[9])
{
    int x, y;
    memset(output, 0, sizeof(Gray));
    for (y = 0; y < IMG_H; ++y) {
        for (x = 0; x < IMG_W; ++x) {
            const double denom = inverse_h[6] * x + inverse_h[7] * y + inverse_h[8];
            double sx, sy;
            int ix, iy;
            if (fabs(denom) < 1e-10) continue;
            sx = (inverse_h[0] * x + inverse_h[1] * y + inverse_h[2]) / denom;
            sy = (inverse_h[3] * x + inverse_h[4] * y + inverse_h[5]) / denom;
            ix = (int)(sx + 0.5);
            iy = (int)(sy + 0.5);
            if (ix >= 0 && ix < IMG_W && iy >= 0 && iy < IMG_H)
                output[y][x] = input[iy][ix];
        }
    }
}

static void print_patch(const char *title, Gray image)
{
    const int x0 = IMG_W / 2 - 15;
    const int y0 = IMG_H / 2 - 4;
    int x, y;
    printf("\n%s (12x8 center sample, origin x=%d y=%d, values scaled to 0..9)\n",
           title, x0, y0);
    for (y = 0; y < 8; ++y) {
        for (x = 0; x < 12; ++x) {
            const int digit = image[y0 + y][x0 + x] * 9 / 256;
            printf("%d ", digit);
        }
        putchar('\n');
    }
}

static int save_stage(const char *dir, const char *name, const uint8_t *data,
                      int width, int height)
{
    if (!save_gray(dir, name, data, width, height)) return 0;
    printf("wrote %-28s %4d x %3d\n", name, width, height);
    return 1;
}

int main(int argc, char **argv)
{
    const char *output_dir = (argc > 1) ? argv[1] : "generated";
    Gray input, intermeans_mask, fixed_mask, row_mask, local_mask;
    Gray ratio_mask, median, gx, gy, magnitude, sobel_mask, trace, ipm;
    Mask otsu_mask, scan_mask, morph_open, morph_close, canny_edges;
    SmallGray reduced;
    uint8_t packed[(PIXELS + 7u) / 8u];
    int16_t left[IMG_H], right[IMG_H];
    uint8_t valid[IMG_H];
    int left_seed = -1, right_seed = -1, stop_rows = 0;
    uint8_t otsu, intermeans;
    int edge_count = 0, trace_steps = 0, x, y;
    const double source_quad[4][2] = {
        {20.0, 8.0}, {59.0, 8.0}, {79.0, 47.0}, {0.0, 47.0}
    };
    const double target_quad[4][2] = {
        {16.0, 0.0}, {63.0, 0.0}, {63.0, 47.0}, {16.0, 47.0}
    };
    double homography[9], inverse_h[9];

    if (!ensure_directory(output_dir)) return EXIT_FAILURE;
    make_synthetic_frame(input);
    threshold_fixed(input, fixed_mask, 128u);
    otsu = otsu_threshold(input);
    threshold_fixed(input, otsu_mask, otsu);
    intermeans = intermeans_threshold(&input[0][0], PIXELS);
    threshold_intermeans(input, intermeans_mask);
    threshold_per_row(input, row_mask);
    threshold_local_mean(input, local_mask, 4, 4);
    difference_ratio_mask(input, ratio_mask, 3, 24u);
    median_3x3(input, median);
    morphology_open(fixed_mask, morph_open);
    morphology_close(fixed_mask, morph_close);
    sobel(input, gx, gy, magnitude);
    sobel_threshold(magnitude, sobel_mask, 26u);
    {
        const int count = canny_simple(input, canny_edges, 80u, 170u);
        printf("Canny strong/weak linked edge pixels: %d / %d\n", count, IMG_W * IMG_H);
    }

    for (y = 0; y < IMG_H; ++y)
        for (x = 0; x < IMG_W; ++x)
            scan_mask[y][x] = fixed_mask[y][x];
    scan_each_row(scan_mask, left, right, valid);

    {
        int16_t inherited_left[IMG_H], inherited_right[IMG_H];
        uint8_t inherited_valid[IMG_H];
        scan_center_inherited(scan_mask, inherited_left, inherited_right, inherited_valid);
        printf("row scan: bottom y=%d valid=%u L=%d R=%d center=%d\n",
               IMG_H - 1, valid[IMG_H - 1], left[IMG_H - 1], right[IMG_H - 1],
               (left[IMG_H - 1] + right[IMG_H - 1]) / 2);
        printf("center inherit: bottom y=%d valid=%u L=%d R=%d\n",
               IMG_H - 1, inherited_valid[IMG_H - 1],
               inherited_left[IMG_H - 1], inherited_right[IMG_H - 1]);
    }

    printf("GRAYSCALE: %d x %d = %lu B\n", IMG_W, IMG_H, (unsigned long)PIXELS);
    printf("BINARY PACKED: %lu B (uint8 mask would be %lu B)\n",
           (unsigned long)((PIXELS + 7u) / 8u), (unsigned long)PIXELS);
    printf("fixed T=128, intermeans T=%u, Otsu T=%u\n", intermeans, otsu);
    printf("difference-ratio Q7 samples: (200,40)=%u, (62,60)=%u\n",
           difference_ratio_q7(200u, 40u), difference_ratio_q7(62u, 60u));
    for (y = 0; y < IMG_H; ++y)
        for (x = 0; x < IMG_W; ++x)
            if (sobel_mask[y][x] != 0u) edge_count++;
    printf("Sobel edge pixels at threshold 26: %d / %d\n", edge_count, IMG_W * IMG_H);

    (void)longest_white_seeds(scan_mask, &left_seed, &right_seed);
    scan_longest_column(scan_mask, left, right, valid,
                        &left_seed, &right_seed, &stop_rows);
    printf("longest-white seeds: L=%d R=%d; search-stop rows=%d\n",
           left_seed, right_seed, stop_rows);

    trace_steps = trace_8_neighbor(sobel_mask, left[IMG_H - 1], IMG_H - 2,
                                   trace, IMG_W * IMG_H);
    printf("8-neighbor trace steps (capped at %d): %d\n", IMG_W * IMG_H, trace_steps);

    downsample_2x(input, reduced);
    printf("2x downsample GRAY8: %d x %d = %d B; loses spatial samples\n",
           OUT_W, OUT_H, OUT_W * OUT_H);
    printf("1-bit packed size: %lu B\n",
           (unsigned long)pack_binary(fixed_mask, packed, sizeof(packed)));

    if (homography_from_quad(source_quad, target_quad, homography) &&
        invert_3x3(homography, inverse_h)) {
        warp_ipm(input, ipm, inverse_h);
        printf("IPM homography calibrated from four point pairs; inverse-map warp complete\n");
    } else {
        memset(ipm, 0, sizeof(ipm));
        puts("IPM homography failed: singular calibration points");
    }

    if (!save_stage(output_dir, "01_gray.pgm", &input[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "02_fixed.pgm", &fixed_mask[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "03_otsu.pgm", &otsu_mask[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "04_intermeans.pgm", &intermeans_mask[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "05_per_row.pgm", &row_mask[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "06_local_mean.pgm", &local_mask[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "07_diff_ratio.pgm", &ratio_mask[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "08_median.pgm", &median[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "09_morph_open.pgm", &morph_open[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "10_morph_close.pgm", &morph_close[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "11_sobel_gx.pgm", &gx[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "12_sobel_gy.pgm", &gy[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "13_sobel_magnitude.pgm", &magnitude[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "14_sobel_edges.pgm", &sobel_mask[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "15_canny_edges.pgm", &canny_edges[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "16_scan.pgm", &scan_mask[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "17_trace8.pgm", &trace[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "18_ipm.pgm", &ipm[0][0], IMG_W, IMG_H) ||
        !save_stage(output_dir, "19_downsample.pgm", &reduced[0][0], OUT_W, OUT_H)) {
        return EXIT_FAILURE;
    }
    print_patch("input", input);
    return EXIT_SUCCESS;
}
