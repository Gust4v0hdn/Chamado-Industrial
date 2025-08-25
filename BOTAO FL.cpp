#include <windows.h>  //usa no ExecultarTI
#include <shellapi.h> //usa no ExecultarTI

POINT ptLastMousePos;
BOOL isDragging = FALSE;
COLORREF currentColor = RGB(255, 35, 35); // Cor inicial (vermelho)
DWORD lastClickTime = 0;
const DWORD doubleClickInterval = GetDoubleClickTime(); // Intervalo para detectar duplo clique

//--------------------------------------------------------------------------------------------------------------
void ExecuteTI() {
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH); // Pega o caminho completo do executável

    // Procura a última barra invertida '\' para separar o nome do executável
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) {
        *(lastSlash + 1) = '\0'; // Corta o caminho logo após a última barra
    }

    strcat(exePath, "Front-end.exe"); // Junta o nome do arquivo com o caminho

    // Verifica se o arquivo existe
    if (GetFileAttributes(exePath) != INVALID_FILE_ATTRIBUTES) {
        ShellExecute(NULL, "open", exePath, NULL, NULL, SW_SHOWNORMAL);
    } else {
        MessageBox(NULL, "O arquivo botchamado.exe não foi encontrado na mesma pasta.", "Erro", MB_ICONERROR);
    }
}

//--------------------------------------------------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static BOOL resetPending = FALSE;

    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH brush = CreateSolidBrush(currentColor);
            FillRect(hdc, &ps.rcPaint, brush);
            DeleteObject(brush);

            SetTextColor(hdc, RGB(255, 255, 255)); // Texto branco
            SetBkMode(hdc, TRANSPARENT);
            DrawText(hdc, "T.I", -1, &ps.rcPaint, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_RBUTTONDOWN:
            if (GetTickCount() - lastClickTime <= doubleClickInterval) {
                // Duplo clique detectado
                if (MessageBox(hwnd, "Deseja realmente fechar o programa?", "Confirmação", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    PostQuitMessage(0); // Fechar o programa
                }
            } else {
                isDragging = TRUE;
                SetCapture(hwnd);
                GetCursorPos(&ptLastMousePos);
            }
            lastClickTime = GetTickCount();
            return 0;

        case WM_MOUSEMOVE:
            if (isDragging) {
                POINT ptCurrentMousePos;
                GetCursorPos(&ptCurrentMousePos);
                int dx = ptCurrentMousePos.x - ptLastMousePos.x;
                int dy = ptCurrentMousePos.y - ptLastMousePos.y;

                RECT rect;
                GetWindowRect(hwnd, &rect);
                MoveWindow(hwnd, rect.left + dx, rect.top + dy, rect.right - rect.left, rect.bottom - rect.top, TRUE);
                ptLastMousePos = ptCurrentMousePos;

                if (currentColor != RGB(255, 100, 0)) {
                    currentColor = RGB(255, 100, 0); // Cor alterada para azul
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            return 0;

        case WM_RBUTTONUP:
            isDragging = FALSE;
            ReleaseCapture();
            if (currentColor == RGB(255, 100, 0)) {
                currentColor = RGB(255, 35, 35); // Volta ao vermelho
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;

        case WM_LBUTTONDOWN:
            currentColor = RGB(128, 128, 128); // Cor cinza ao clicar
            InvalidateRect(hwnd, NULL, TRUE);
            ExecuteTI();
            SetTimer(hwnd, 1, 200, NULL); // Restaurar a cor após 200ms
            resetPending = TRUE;
            return 0;

        case WM_TIMER:
            if (resetPending) {
                currentColor = RGB(255, 35, 35); // Cor vermelha de volta
                InvalidateRect(hwnd, NULL, TRUE);
                KillTimer(hwnd, 1);
                resetPending = FALSE;
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "FloatingButton";

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Falha ao registrar a classe!", "Erro", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        "Floating Button",
        WS_POPUP,
        -20, 450,
        100, 30,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBox(NULL, "Falha ao criar a janela!", "Erro", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

