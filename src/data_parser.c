#include "../inc/pathfinder.h"

static t_data *exit_error(t_data **data, int fd) {
    if (data && *data) {  // Перевірка на існування даних
        free_resources(*data);  // Функція для очищення пам'яті
        *data = NULL;           // Уникаємо можливого повторного використання
    }
    close(fd);  // Закриття файлу
    return NULL;  // Завжди повертаємо NULL
}

// Функції для роботи з вузлами
static t_node *create_node(int index, char *name) {
    t_node *result = malloc(sizeof(t_node));
    result->name = name;
    result->id = index;
    return result;
}

static int id_by_name(t_list **nodes_list, const char *name) {
    if (!*nodes_list)
        mx_push_back(nodes_list, create_node(0, mx_strdup(name)));

    int id = 0;
    for (t_list *copy = *nodes_list; copy; copy = copy->next) {
        if (mx_strcmp(((t_node *)copy->data)->name, name) == 0) {
            return ((t_node *)copy->data)->id; // Повертає ідентифікатор
        }
        ++id;
    }

    mx_push_back(nodes_list, create_node(id, mx_strdup(name)));
    return id;
}

// Перевіряє, чи слово складається лише з алфавітних символів
static bool is_word_alphabetical(const char *word) {
    if (word == NULL) return false;
    for (int i = 0; word[i] != '\0'; i++) {
        if (!mx_isalphabetical(word[i])) return false;
    }
    return true;
}

// Функція обробки рядка
static char **handle_line(const char *line, int line_num) {
    int d1i = mx_get_char_index(line, '-');
    int d2i = mx_get_char_index(line, ',');
    if (d1i < 1 || d2i < d1i || d2i > mx_strlen(line) - 2 || d1i + 1 == d2i) {
        handle_error(LINE, NULL, line_num);
        return NULL;
    }
    char **arr = malloc(4 * sizeof(char *));
    arr[0] = mx_strndup(line, d1i);
    arr[1] = mx_strndup(line + d1i + 1, d2i - d1i - 1);
    arr[2] = mx_strndup(line + d2i + 1, mx_strlen(line) - d2i - 1);
    arr[3] = NULL;
    int len = mx_atoi(arr[2]);

    if (mx_strlen(arr[0]) < 1 || mx_strlen(arr[1]) < 1 || mx_strcmp(arr[0], arr[1]) == 0
        || !is_word_alphabetical(arr[0]) || !is_word_alphabetical(arr[1])
        || (((mx_strlen(arr[2]) != mx_numlen(len) || len < 1)) && len != -2)) {
        mx_del_strarr(&arr);
        handle_error(LINE, NULL, line_num);
        return NULL;
    }
    return arr;
}

// Функція обробки рядка файлу
static char **process_line(int fd, int *eof, const int *num) {
    int size = 0;
    char *line = read_line_delim(fd, &size, eof, '\n');

    // Перевірка: Якщо кінець файлу, але є дані у рядку
    if (*eof > 0 && size > 0) {
        char **arr = handle_line(line, *num);
        free(line);
        return arr;
    }

    // Якщо порожній рядок або EOF
    if (*eof > 0 || size == 0) {
        free(line);
        return NULL;
    }

    char **arr = handle_line(line, *num);
    free(line);
    return arr;
}

// Функція обробки ідентифікаторів
static int process_ids(t_data **data, char **names, int *i, int *j) {
    *i = id_by_name(&(*data)->nodes, names[0]);
    *j = id_by_name(&(*data)->nodes, names[1]);
    if (*i >= (*data)->graph->size || *j >= (*data)->graph->size) {
        (*data)->error = ISLANDS;
        return 0;
    }
    return (*i != -1 && *j != -1);
}

// Перевіряє довжину мосту
static int check_bridge_length(char *len_str, int num, t_data *data) {
    int len = mx_atoi(len_str);
    if (len == -2 && data->error >= LENGTH) {
        data->error = LENGTH;
        return 0;
    } else if (len < 1) {
        data->error = LINE;
        return handle_error(LINE, NULL, num);
    }
    return len;
}

// Оновлює матрицю графа
static void update_graph_matrix(t_data **data, int i, int j, int len) {
    if ((*data)->graph->matrix[i][j] != INT_MAX) { // Якщо міст вже існує
        if ((*data)->graph->matrix[i][j] != len) { // І довжини мосту не збігаються
            (*data)->error = BRIDGES;             // Встановлюємо помилку "дубльований міст"
            return;
        }
        return; // Якщо міст існує і довжина збігається, нічого не змінюємо
    }

    // Якщо моста ще немає, додаємо його в матрицю
    (*data)->graph->matrix[i][j] = len;
    (*data)->graph->matrix[j][i] = len;
}
    
// Основна функція парсингу
t_data *parse_input_file(char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        handle_error(NAME, filename, 0);
        return NULL;
    }

    t_data *data = initialize_data(fd, filename);
    if (!data) return exit_error(NULL, fd);

    int eof = 0, num = 2;
    long long int sum = 0;

    while (1) {
        char **arr = process_line(fd, &eof, &num);

        if (!arr && eof) break;

        if (!arr) return exit_error(&data, fd);

        int i, j;
        if (!process_ids(&data, arr, &i, &j)) {
            mx_del_strarr(&arr);
            return exit_error(&data, fd);
        }

        int len = check_bridge_length(arr[2], num, data);
        mx_del_strarr(&arr);
        if (len < 0) return exit_error(&data, fd);

        update_graph_matrix(&data, i, j, len);
        sum += len;
        num++;
    }

    if (sum > INT_MAX) {
        data->error = LENGTH;
        return exit_error(&data, fd);
    }

    int node_count = mx_list_size(data->nodes);
    if (node_count != data->amount) {
        handle_error(ISLANDS, NULL, 0);
        return exit_error(&data, fd);
    }

    // **Ця перевірка повинна бути в тілі функції**
    if (data->error == BRIDGES) {
        handle_error(BRIDGES, NULL, 0);
        return exit_error(&data, fd);
    }

    close(fd);
    return data;
}
