# Chamado-Industrial
Este é um sistema de automação para relatórios de problemas, dividido em componentes. O BOTAO FL.cpp inicia o front-end Front-end.py, que coleta dados do erro e os salva no mensagem.json. Então, dois backends, enviodedados.c e Back-end.py, usam esses dados para notificar via Telegram e criar um ticket no GLPI. O registro.log grava todas as ações.
