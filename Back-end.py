import json
import os
import socket
import platform
import requests
import sys

# ========================== CONFIGURAÇÃO GLPI ==========================
GLPI_URL = "https://glpi.naturafrig.com.br/apirest.php"
USER_TOKEN = "gN1su8ncyuzjYfARSpfTIxFnzrSaoqMaSzC7uYEQ"
APP_TOKEN = "ikUmn3q8qe15j7Z6OrreQx63TxMmGYxIyQ8JZ91r"

# ========================== ARQUIVO JSON ===============================
caminho_json = "mensagem.json"

try:
    with open(caminho_json, "r", encoding="utf-8") as f:
        dados = json.load(f)
except FileNotFoundError:
    print(f"❌ Arquivo não encontrado: {caminho_json}")
    sys.exit(1)
except json.JSONDecodeError as e:
    print(f"❌ Erro ao decodificar JSON: {e}")
    sys.exit(1)

# ========================== MENSAGENS PERSONALIZADAS =========================
mensagens_personalizadas = {
    "IMPRESSÃO COM FALHA": (
        "O usuário {usuario} do ambiente de trabalho {hostname} está relatando um problema em "
        "'FALHA DE IMPRESSAO' na impressora Zebra. Algo está com defeito e precisa de verificação."
    ),
    "SISTEMA COM FALHA": (
        "O sistema está apresentando falhas operacionais críticas. Usuário {usuario}, usando o computador {hostname}, "
        "informou erros ao executar funcionalidades básicas do software interno. Ação imediata necessária."
    ),
    "ETIQUETA COM FALHA": (
        "Usuário {usuario}, na estação {hostname}, relata que a etiqueta  está com falha ao imprimir "
        "e dificultando o processo."
    ),
    "ROLETE GRUDANDO": (
        "Usuário {usuario}, na estação {hostname}, relata que o rolete da impressora está grudando "
        "e dificultando o processo de impressão de etiquetas."
    ),
    "BALANCA": (
        "Falha na comunicação com a balança. O usuário {usuario}, no PC {hostname}, relatou instabilidade ou ausência de leitura "
        "no equipamento de pesagem conectado."
    ),
    "COMPUTADOR": (
        "O computador {hostname}, utilizado por {usuario}, está apresentando travamentos, lentidão ou falhas ao inicializar. "
        "Possível necessidade de manutenção."
    )
}

titulos_resumidos = {
    "IMPRESSÃO COM FALHA": "Falha na impressora de etiqueta",
    "SISTEMA COM FALHA": "Falha no sistema",
    "ETIQUETA COM FALHA": "Falha na etiqueta",
    "ROLETE GRUDANDO": "Rolete grudando",
    "BALANCA": "Erro na balança",
    "COMPUTADOR": "Falha no computador"
}

descricao_individual = {
    "IMPRESSÃO COM FALHA": "Impressora Zebra com falha de impressão contínua.",
    "SISTEMA COM FALHA": "Erro crítico no sistema operacional ou software da empresa.",
    "ETIQUETA COM FALHA": "Etiqueta não está imprimindo corretamente ou está com defeito.",
    "ROLETE GRUDANDO": "Etiqueta não avança corretamente devido a rolete grudando.",
    "BALANCA": "Falha de comunicação ou leitura na balança de pesagem.",
    "COMPUTADOR": "Estação com lentidão excessiva ou falha de inicialização."
}

# ========================== EXTRAÇÃO SEGURA DOS DADOS ==================
# Pega o erro original do JSON. Se for "OUTRO", o valor de SDED já será a mensagem personalizada.
erro_do_json = dados.get("SDED", "ERRO NÃO IDENTIFICADO")
usuario = dados.get("USUARIO", os.getlogin())
hostname = dados.get("HOSTNAME", socket.gethostname())
data = dados.get("DATA", "")
hora = dados.get("HORA", "")

# Usamos a versão em maiúsculas para procurar nas mensagens predefinidas
erro_para_lookup = erro_do_json.upper()

# ========================== MONTAGEM DA MENSAGEM FINAL ========================
urgencia_valor = 3
urgencias_texto = {
    1: "Muito Baixo",
    2: "Baixo",
    3: "Média",
    4: "Alta",
    5: "Muito Alta"
}
urgencia_texto = urgencias_texto.get(urgencia_valor, "Média")

# Lógica para definir título, observação e descrição com base no erro
if erro_para_lookup in mensagens_personalizadas:
    # Se o erro está nas mensagens predefinidas
    titulo = titulos_resumidos.get(erro_para_lookup, "Erro técnico")
    observacao = mensagens_personalizadas[erro_para_lookup].format(usuario=usuario, hostname=hostname)
    descricao7 = descricao_individual.get(erro_para_lookup, "Problema técnico detectado pelo usuário.")
else:
    # Se o erro não está nas mensagens predefinidas (inclui "OUTRO" com mensagem personalizada)
    titulo = f"Chamado do usuário {usuario}: {erro_do_json}"
    observacao = f"O usuário {usuario} no computador {hostname} relatou o seguinte problema: {erro_do_json}"
    descricao7 = erro_do_json # A própria mensagem personalizada é a descrição

conteudo_chamado = (
    "Dados do formulário\n"
    "Título e Identificação da Solicitação\n"
    f"1) Descrição Reduzida da Solicitação : {titulo}\n"
    "2) Tipo : Requisição\n"
    f"3) Urgência : {urgencia_texto}\n"
    f"4) Data de Abertura : {data}\n"
    f"5) Estação de Trabalho : {hostname}\n"
    f"6) Usuario : {usuario}\n\n"
    "Descrição do Chamado\n"
    f"7) {descricao7}\n\n"
    "Complemento do Chamado.\n"
    f"8) Observações. : \n{observacao}\n\n"
    "Contato do Suporte/TI\n"
    "10) Entre em Contato Imediatamente com o Suporte/TI :"
)

ticket_data = {
    "name": f"{titulo} - {usuario}", # O nome do ticket também será mais descritivo
    "content": conteudo_chamado,
    "itilcategories_id": 45,
    "type": 2,
    "urgency": 5,
    "impact": 5,
    "priority": 3,
    "users_id_requester": 56
}

# ========================== FUNÇÕES GLPI API ============================

def init_session():
    url = f"{GLPI_URL}/initSession"
    headers = {
        "Authorization": f"user_token {USER_TOKEN}",
        "App-Token": APP_TOKEN
    }
    try:
        response = requests.post(url, headers=headers)
        response.raise_for_status()
        session_token = response.json().get("session_token")
        print("✅ Sessão iniciada.")
        return session_token
    except requests.exceptions.RequestException as e:
        print(f"❌ Erro ao iniciar sessão: {e}")
        return None

def create_ticket(session_token, ticket_data):
    url = f"{GLPI_URL}/Ticket"
    headers = {
        "Session-Token": session_token,
        "App-Token": APP_TOKEN,
        "Content-Type": "application/json"
    }
    try:
        response = requests.post(url, headers=headers, json={"input": ticket_data})
        response_data = response.content.decode('utf-8-sig')
        json_response = json.loads(response_data)
        if response.status_code == 201:
            print("✅ Chamado criado com sucesso.")
            print(json.dumps(json_response, indent=2, ensure_ascii=False))
            return json_response
        else:
            print(f"❌ Erro ao criar chamado: {json_response}")
            return None
    except Exception as e:
        print(f"❌ Erro ao processar resposta: {e}")
        print("🔁 Resposta:", response.text)
        return None

def kill_session(session_token):
    url = f"{GLPI_URL}/killSession"
    headers = {
        "Session-Token": session_token,
        "App-Token": APP_TOKEN
    }
    try:
        response = requests.post(url, headers=headers)
        if response.status_code == 200:
            print("✅ Sessão encerrada.")
        else:
            print(f"❌ Erro ao encerrar sessão: {response.text}")
    except Exception as e:
        print(f"❌ Erro ao encerrar sessão: {e}")

# ========================== EXECUÇÃO PRINCIPAL ===========================

if __name__ == "__main__":
    session_token = init_session()
    if not session_token:
        sys.exit(1)

    resultado = create_ticket(session_token, ticket_data)
    if not resultado:
        print("❌ O chamado não foi criado. Verifique os dados.")
    else:
        print("🎯 Tudo certo! Chamado enviado.")

    kill_session(session_token)
