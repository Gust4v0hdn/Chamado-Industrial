# Chamado-Industrial
This is an automated system for reporting problems, divided into components. The **`BOTAO FL.cpp`** file starts the **`Front-end.py`** front end, which collects error data and saves it to **`mensagem.json`**. Then, two back ends, **`enviodedados.c`** and **`Back-end.py`**, use this data to send notifications via Telegram and create a ticket in GLPI. The **`registro.log`** file records all actions.
This code functions as a ticket simplifier in the IT sector, facilitating quick and precise communication in areas that require immediate attention. It operates

Here is an in-depth analysis of each component, focusing on its technical implementation.

---

- **`BOTAO FL.cpp` - (The Application Launcher)**
    
    ---
    
    <aside>
    
    This module is a Win32 application (Windows API) with a message loop architecture.This module is a Win32 application (Windows API) with a message loop architecture.
    
    </aside>
    
    This is a C++ program compiled for Windows operating system. Its main objective is to provide a minimalist and floating user interface.
    
    **GUI Implementation**: Uses the `WNDCLASS` structure and the `CreateWindowEx` function with the style `WS_EX_TOPMOST | WS_EX_TOOLWINDOW` to create a pop-up window without a title bar that always remains on top of other windows.
    
    ![[image (2).png]]
    
1. **Event Management**: The `WindowProc` function acts as a [**`<h color="default-background"><h color="default">callback</h></h>`**](https://www.youtube.com/watch?v=gcE0Gx6TgT4) that processes messages from the operating system's event queue.
- **Drag-and-Drop**: Handling the `WM_LBUTTONDOWN` event and capturing mouse messages (`SetCapture`) allows the user to drag the window. The logic is based on the difference between the current mouse position and the initial position (`ptLastMousePos`).
- **Application Launch**: The double-click logic is implemented manually. Upon receiving `WM_LBUTTONDBLCLK`, it calls the `ExecuteTI()` function.
- **Process Launch**: The `ExecuteTI()` function uses the `GetModuleFileName()` API to obtain the full path of the executable (`.exe`) at runtime. It then manipulates the path string to locate the executable directory, appends the name `"Front-end.exe"`, and uses the `ShellExecute` function to start a new process. This ensures that the Python front-end is executed regardless of the floating button's location.

---

### **2. `Front-end.py` - Data Collector (Python)**

This script uses the native `tkinter` library for GUI construction and interacts with the file system and network.

- **GUI Architecture**: The script creates a minimalist interface with buttons that represent problems. Each button is associated with a *handler* function (`on_click`) that is triggered by mouse events.
- 
   ![image (3).png](attachment:82bc5deb-2c20-4a4f-8be5-7b29256218d3:image.png)
  
- **Connectivity Verification**: The `check_internet_connection()` method uses the `socket` library for low-level network connectivity verification.
    - It attempts to establish a TCP connection (`socket.create_connection`) with the address `www.google.com` on port 80, with a `timeout` of 6 seconds.
    - The `update_status_indicator()` is executed in a separate *thread* using `threading.Thread`, which prevents the GUI from freezing during network checking.
    
    ![image (3).png](attachment:82bc5deb-2c20-4a4f-8be5-7b29256218d3:image.png)
    
- **JSON Handling**:
    - The `on_click()` function collects information from the operating system, such as the host name (`platform.node()`).
    - It builds a dictionary (`payload`) and uses `json.dump()` to serialize this Python object into a JSON *string*.
    - This *string* is then written to the `mensagem.json` file in overwrite mode (`"w"`). The `"STATUS": false` field is essential to indicate to backends that a new error is awaiting processing.
    
    ---
    

### **3. `mensagem.json` - Shared Data Layer**

This file is a JSON object that serves as a communication state between asynchronous processes.

- **Schema Structure**: Contains semantic keys: `"SDED"` (Error Description), `"DATA"`, `"HORA"` (Time), `"HOSTNAME"`, and `"STATUS"`.
- **State Semaphore**: The `"STATUS"` field acts as a binary semaphore.
    - `false`: Indicates that the error has not yet been processed by one of the backends.
    - `true`: Indicates that the error has been processed, preventing data re-sending.
    
    ---
    

### **4. `enviodedados.c` - Notification Backend (C)**

This is a C program compiled for Windows that handles low-level network communication and API interactions.

- **Network Management**:
    - Uses `WSAStartup()` and `WSACleanup()` to initialize and finalize the Winsock API, respectively.
    - Connectivity verification is done with `GetIpForwardTable()`, which enumerates IP routing tables to confirm the existence of an active connection.
- **JSON Analysis**: The program reads `mensagem.json` and uses the `cJSON` library to parse the content.
    - It accesses specific JSON fields, such as `"SDED"`, `"HOSTNAME"`, `"DATA"`, and `"HORA"`, to extract error data.
    - Checking the `STATUS` field is crucial for processing logic.
- **External API Communication**:
    - **Telegram**: Uses the `wininet.lib` library and the functions `InternetOpen()`, `InternetOpenUrl()`, and `InternetReadFile()` to perform an HTTP GET request to the Telegram API, sending the formatted message as a URL parameter.
    - **Internal Server**: In parallel, it uses raw *sockets* (`socket()`, `connect()`, `send()`) for low-level communication with a local server, sending a simple data *buffer* (`"buzz"`).
- **Log Management**: The `salvarLog()` function ensures that each crucial step of the process is recorded in the `registro.log` file, including *timestamps* and descriptive messages.

---

### **5. `Back-end.py` - Ticketing Backend (Python)**

This Python script uses the `requests` library to interact with the GLPI RESTful API, automating ticket creation.

- **API Authentication**:
    - The script defines constants for the GLPI URL and authentication tokens (`USER_TOKEN`, `APP_TOKEN`).
    - The `init_session()` function makes a POST request to the `/initSession` endpoint with the tokens in the *headers*. The response contains the `Session-Token` that is used for all subsequent interactions, ensuring that the session is authenticated.
- **Data Processing and Ticket Creation**:
    - Reads `mensagem.json` and `caminho_json` with `json.load()`.
    - The `mensagens_personalizadas` dictionary is used as a *template* to generate the ticket description.
    - The `create_ticket()` function builds the ticket data object in a Python dictionary and sends it as JSON (`json=ticket_data`) in a POST request to the `/Ticket` endpoint.
- **Session Encapsulation**: The `kill_session()` function is called at the end to terminate the API session.

# Codes

---

- Frontend
- Backend (GLPI)
    
    [mensagem.json](attachment:8fe86fda-a0a2-487f-8da9-307205a26238:Back-end.py)
    
- Backend (Telegram and PC)
    
    [enviodedados.c](attachment:ef0912de-cea2-41d6-9918-c7bad1d924e7:enviodedados.c)
    
- Floating Button
    
    [BOTAO FL.cpp](attachment:72ad55f2-ea75-4a94-87b5-fc6caf89b6a0:BOTAO_FL.cpp)
    
- Json Library
    
    [cJSON.c](attachment:f468f78c-3477-4037-ac67-0be0d1075822:Front-end.py)
    
- Files (Json and Log)
    
    [registro.log](attachment:f484c312-b982-40b9-b810-518d983dc1b9:registro.log)
    
    Icons
    

```bash
|-------icone.png
|
|-------BOTAO FL.exe
|
|-------Front-end.exe
|
|-------status.exe
|
|-------Back-end.exe
|
|-------mensagem.json
|
|-------enviodedados.exe

```
