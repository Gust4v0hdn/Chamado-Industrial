import tkinter as tk
from tkinter import ttk
from tkinter import messagebox
import json
import datetime
import socket
import os
import subprocess
import threading
import sys

# Lista de erros
erros = [
    "IMPRESSÃO COM FALHA",
    "ROLETE GRUDANDO",
    "ETIQUETA COM FALHA",
    "SISTEMA COM FALHA",
    "BALANÇA",
    "COMPUTADOR",
    "OUTRO (Digite a mensagem)"
]

# Lista global para armazenar os botões
botoes = []

# --- Funções de Internet e Status ---

def check_internet_connection():
    """Verifica a conexão com a internet."""
    try:
        # Tenta se conectar a um site popular com um timeout de 5 segundos.
        socket.create_connection(("www.google.com", 80), timeout=6)
        return True
    except OSError:
        return False

def update_status_indicator():
    """Atualiza o indicador visual de status da internet e o estado dos botões."""
    if check_internet_connection():
        status_canvas.itemconfig(status_circle, fill="green", outline="green")
        status_label.config(text="ONLINE", fg="green")
        for btn in botoes:
            btn.config(state="normal") # Ativa os botões
    else:
        status_canvas.itemconfig(status_circle, fill="red", outline="red")
        status_label.config(text="OFFLINE", fg="red")
        for btn in botoes:
            btn.config(state="disabled") # Desativa os botões
    
    # Agenda a próxima verificação para daqui a 5 segundos (5000ms)
    root.after(5000, update_status_indicator)

# ... (o restante das funções `on_click`, `solicitar_mensagem_personalizada` e `executar_outros_programas`
#     permanecem inalteradas, pois o problema não está nelas) ...

def on_click(erro):
    # Dicionário para armazenar os dados que serão atualizados
    dados_a_atualizar = {}

    if "OUTRO" in erro:
        erro_personalizado = solicitar_mensagem_personalizada()
        if not erro_personalizado:
            return  # Cancelado ou vazio
        dados_a_atualizar["SDED"] = erro_personalizado # Atualiza apenas SDED
    else:
        dados_a_atualizar["SDED"] = erro # Atualiza SDED com o erro selecionado

    # Obtém a data e hora atuais
    current_time = datetime.datetime.now()
    dados_a_atualizar["DATA"] = current_time.strftime("%d/%m/%Y")
    dados_a_atualizar["HORA"] = current_time.strftime("%H:%M")
    dados_a_atualizar["HOSTNAME"] = socket.gethostname()
    dados_a_atualizar["STATUS"] = False

    # --- Lógica para atualizar o JSON ---
    caminho_arquivo_json = "mensagem.json"
    dados_existente = {}

    # 1. Tenta ler o conteúdo existente do JSON
    if os.path.exists(caminho_arquivo_json):
        try:
            with open(caminho_arquivo_json, "r", encoding="utf-8") as f:
                dados_existente = json.load(f)
        except json.JSONDecodeError:
            messagebox.showwarning("Aviso", "O arquivo mensagem.json está corrompido ou vazio. Criando um novo.")
            dados_existente = {} # Reseta se estiver corrompido
        except IOError as e:
            messagebox.showerror("Erro", f"Não foi possível ler o arquivo mensagem.json: {e}")
            return # Sai da função se não conseguir ler

    # 2. Atualiza os dados existentes com os novos dados
    dados_existente.update(dados_a_atualizar)

    # 3. Escreve os dados de volta no arquivo
    try:
        with open(caminho_arquivo_json, "w", encoding="utf-8") as f:
            json.dump(dados_existente, f, indent=4, ensure_ascii=False)
        
        # Chama a função para executar os outros programas.
        executar_outros_programas()

        # Fecha a janela principal
        root.destroy()
    except IOError as e:
        messagebox.showerror("Erro", f"Não foi possível salvar o arquivo: {e}")
    # --- Fim da lógica para atualizar o JSON ---


def solicitar_mensagem_personalizada():
    top = tk.Toplevel(root)
    top.title("Mensagem Personalizada")
    top.geometry("370x180")
    top.resizable(False, False)
    top.configure(bg="#f0f0f0")

    # Centralizar sobre a janela principal
    top.update_idletasks()
    x = root.winfo_x() + (root.winfo_width() // 2) - (top.winfo_width() // 2)
    y = root.winfo_y() + (root.winfo_height() // 2) - (top.winfo_height() // 2)
    top.geometry(f"+{x}+{y}")

    resultado = {"mensagem": None}

    tk.Label(top, text="Digite a mensagem personalizada:",
             font=("Segoe UI", 12), bg="#f0f0f0").pack(pady=(15, 5))

    entry = tk.Entry(top, font=("Segoe UI", 12), width=35)
    entry.pack(pady=5)
    entry.focus()

    # Botões
    btn_frame = tk.Frame(top, bg="#f0f0f0")
    btn_frame.pack(pady=15)

    def confirmar():
        texto = entry.get().strip()
        if texto:
            resultado["mensagem"] = texto
            top.destroy()
        else:
            messagebox.showwarning("Aviso", "A mensagem não pode estar vazia.")

    def cancelar():
        top.destroy()

    tk.Button(btn_frame, text="Confirmar", font=("Segoe UI", 11, "bold"),
              bg="#4a90e2", fg="white", activebackground="#357ab8",
              relief="flat", width=12, command=confirmar).pack(side="left", padx=10)

    tk.Button(btn_frame, text="Cancelar", font=("Segoe UI", 11),
              bg="#cccccc", fg="black", activebackground="#aaaaaa",
              relief="flat", width=12, command=cancelar).pack(side="left", padx=10)

    top.grab_set()
    top.wait_window()
    return resultado["mensagem"]


def executar_outros_programas():
    """Função para chamar os outros arquivos .exe."""
    
    # Verifica se o programa está sendo executado como um executável (empacotado)
    if getattr(sys, 'frozen', False):
        diretorio_atual = os.path.dirname(sys.executable)
    else:
        # Se for um script .py normal, usa o método original
        diretorio_atual = os.path.dirname(os.path.abspath(__file__))

    executaveis = ["enviodedados.exe", "status.exe", "Back-end.exe"]

    for exe_name in executaveis:
        caminho_do_exe = os.path.join(diretorio_atual, exe_name)
        
        if os.path.exists(caminho_do_exe):
            try:
                subprocess.Popen([caminho_do_exe], creationflags=subprocess.CREATE_NEW_CONSOLE)
            except Exception as e:
                messagebox.showerror("Erro", f"Não foi possível executar {exe_name}: {e}")
        else:
            messagebox.showwarning("Aviso", f"O arquivo {exe_name} não foi encontrado no mesmo diretório.")
    # Inicia a sessão na API GLPI
# Janela principal
root = tk.Tk()
root.title("Chamados TI")
root.iconphoto(True, tk.PhotoImage(file="icone.png"))
root.configure(bg="#f3f3f3")
root.geometry("350x560")
root.resizable(False, False)

# --- Adicionando o Indicador de Status com Label ---
status_frame = tk.Frame(root, bg="#f3f3f3")
status_frame.pack(side="top", fill="x", pady=5, padx=10)

status_label = tk.Label(status_frame, text="", font=("Segoe UI", 10, "bold"), bg="#f3f3f3")
status_label.pack(side="right", padx=(0, 5))

status_canvas = tk.Canvas(status_frame, width=20, height=20, bg="#f3f3f3", highlightthickness=0)
status_canvas.pack(side="right")

status_circle = status_canvas.create_oval(5, 5, 15, 15, fill="gray", outline="gray")

# Inicia a verificação de status da internet
update_status_indicator()

# Estilo visual
style = ttk.Style()
style.theme_use("clam")
style.configure(
    "Botao.TButton",
    font=("Segoe UI", 13, "bold"),
    foreground="#ffffff",
    background="#4a90e2",
    padding=14,
    borderwidth=0,
)
style.map(
    "Botao.TButton",
    background=[("active", "#357ab8")],
)

# Frame central
frame = tk.Frame(root, bg="#f3f3f3")
frame.pack(expand=True)

# Título (espaço reduzido com pady)
tk.Label(
    frame,
    text="SELECIONE O ERRO",
    font=("Segoe UI", 18, "bold"),
    bg="#f3f3f3",
    fg="#333333"
).pack(pady=(5, 10)) 

# Botões
for erro in erros:
    btn = ttk.Button(
        frame,
        text=erro,
        style="Botao.TButton",
        command=lambda e=erro: on_click(e)
    )
    btn.pack(fill="x", padx=40, pady=5)
    botoes.append(btn)

# Rodapé com a versão
tk.Label(
    root,
    text="Sistema de Chamados v2.0",
    font=("Segoe UI", 10),
    bg="#f0f0f0",
    fg="#888"
).pack(pady=10)

# Iniciar interface
root.mainloop()