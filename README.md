# Calculadora Cliente–Servidor com Sockets em C
Bruna Aguiar Muchiuti  
RA: 10418358


## Objetivo
Implementar uma aplicação **cliente–servidor** em C utilizando **sockets TCP (IPv4)**.  
O cliente envia operações matemáticas e o servidor processa e devolve o resultado seguindo um **protocolo textual simples**.  

Usei nesse projeto:  
- Programação em redes (socket TCP).  
- Parsing robusto de mensagens.  
- Tratamento de erros e boas práticas de E/S.  
- Organização de código em módulos.  
- Automação de build com Makefile.  

---

## Estrutura do Projeto
```
/include
  proto.h       # Cabeçalhos e definições comuns
/src
  server.c      # Implementação do servidor
  client.c      # Implementação do cliente
Makefile        # Automação de compilação
run.sh          # Script para compilar e executar
README.md       # Este documento
```

---

## Protocolo Definido
### Formato de requisição
```
OP A B\n
```
- `OP ∈ {ADD, SUB, MUL, DIV}`  
- `A` e `B` são números reais no formato decimal com ponto (ex.: `2`, `-3.5`, `10.0`).  

Exemplo:
```
ADD 10 2\n
```

### Formato de resposta
```
OK R\n
```
ou, em caso de erro:
```
ERR <COD> <mensagem>\n
```

Códigos de erro:  
- `EINV` → entrada inválida.  
- `EZDV` → divisão por zero.  
- `ESRV` → erro interno.  

### Forma alternativa (infixa)
O servidor também aceita:
```
A <op> B\n
```
com `<op> ∈ {+, -, *, /}`  

Exemplo:
```
10 + 2\n   ->   OK 12
```

### Encerramento
- Cliente envia `QUIT\n` → servidor fecha a conexão.  

---

## Compilação
Com o Makefile:
```bash
make clean     # remove binários antigos
make all       # compila servidor e cliente
```

Executáveis gerados:
- `server`  
- `client`

---

## Execução
### Servidor
Roda na porta padrão **5050**, mas pode receber outra via linha de comando:
```bash
./server          # porta 5050
```

### Cliente
Conecta ao servidor:
```bash
./client <ip-servidor> <porta>
```

Exemplo:
```bash
./client 127.0.0.1 5050
```

### Exemplo de interação
```
ADD 10 2
OK 12
SUB 7 9
OK -2
DIV 5 0
ERR EZDV divisao_por_zero
10 * 3.5
OK 35
QUIT
```

---

## Explicação dos Arquivos

### proto.h
Arquivo de cabeçalho central do projeto. Aqui ficam:
- Constantes (`DEFAULT_PORT`, `LINE_MAX`, códigos de erro como `EINV`, `EZDV`).  
- Definições de tipos (`enum op_t` com as operações e `resultado_parse_t` para representar o resultado do parsing).  
- Declarações das funções auxiliares utilizadas em todo o projeto.  

#### Funções declaradas:
- **`remove_spaces()`**  
  - Responsável por limpar a string recebida, removendo espaços em branco no início e no final.  
  - Também elimina caracteres de quebra de linha `\n` e `\r`.  
  - Essa etapa garante que a entrada esteja padronizada antes do parsing.  

- **`set_c_locale()`**  
  - Força o programa a usar o locale `"C"` para números.  
  - Isso garante que o separador decimal seja sempre `.` (ponto), independente da configuração do sistema operacional do usuário.  

- **`format_number()`**  
  - Recebe um número `double` e converte para string no formato `"%.6f"`.  
  - Em seguida, remove zeros à direita e o ponto final se não houver casas decimais relevantes.  
  - Exemplo: `12.000000` → `"12"`, `3.500000` → `"3.5"`.  

- **`parse_request_line()`**  
  - Interpreta a linha enviada pelo cliente.  
  - Primeiro tenta o formato **prefixo** (`ADD 10 2`).  
  - Se não funcionar, tenta o formato **infixo** (`10 + 2`).  
  - Retorna um `resultado_parse_t`, indicando se a entrada é válida, a operação identificada e os operandos.  
  - Se a entrada for inválida, já devolve preenchido com código e mensagem de erro (`EINV entrada_invalida`).  

---

### server.c
Implementa o **servidor** TCP.  

#### Principais pontos:
- **Tratamento de sinais (`install_signal_handlers`)**  
  - Registra handlers para `SIGINT`, `SIGTERM`, `SIGQUIT`.  
  - Isso garante que, ao encerrar o programa com `Ctrl+C`, o servidor feche os sockets de forma limpa.  

- **Concorrência com `fork()`**  
  - Para cada cliente aceito, o servidor cria um processo filho que atende aquele cliente de forma independente.  
  - O processo pai continua escutando novas conexões.  

- **Função `serve_client()`**  
  - Lê uma linha do cliente (`safe_readline`).  
  - Faz parsing com `parse_request_line()`.  
  - Caso seja `QUIT`, encerra a sessão.  
  - Se for uma operação matemática, executa o cálculo.  
  - Se for erro, responde com `ERR`.  
  - Sempre envia resposta ao cliente com `send_all()`.  

- **Tratamento de erros no servidor**  
  - Entrada inválida: `ERR EINV entrada_invalida`.  
  - Divisão por zero: `ERR EZDV divisao_por_zero`.  
  - Erros internos: `ERR ESRV erro_interno`.  

---

### client.c
Implementa o **cliente** TCP.  

#### Principais pontos:
- Conecta ao servidor usando `socket()` e `connect()`.  
- Mostra mensagem ao usuário para digitar operações.  
- Lê entrada do **stdin** e envia ao servidor (`send_all`).  
- Recebe a resposta (`recv_line`) e imprime no **stdout**.  
- Se o usuário digitar `QUIT`, envia para o servidor e encerra a conexão.  

---

### Makefile
Automatiza a compilação do projeto.  
- `make all` → compila `server` e `client`.  
- `make server` → compila apenas o servidor.  
- `make client` → compila apenas o cliente.  
- `make clean` → remove binários.    

---

### run.sh
Script auxiliar para facilitar a execução.  
- Pode chamar `make clean && make all`.  
- Pode rodar o servidor em background e já abrir o cliente.  
- Garante que a correção/teste seja simples: apenas `./run.sh`.  

Para executar:
```bash
chmod +x run.sh
./run.sh
```

---

## Decisões de Projeto
- **Parsing estrito**: exige que a entrada esteja no formato definido (prefixo ou infixo). Isso evita ambiguidades.  
- **Locale fixo**: garante que números usem sempre `.` como separador decimal, independente da máquina.  
- **Concorrência**: implementação com `fork()`, permitindo múltiplos clientes simultâneos.  
- **Tratamento de erros**: respostas padronizadas em caso de entrada inválida, divisão por zero ou falhas internas.  
- **Organização modular**: separação clara entre cabeçalhos, servidor, cliente e automação com Makefile.  

---

## Exemplos Detalhados

| Entrada (cliente) | Saída (servidor)          |
|-------------------|---------------------------|
| `ADD 5 5`         | `OK 10`                  |
| `SUB 1 2`         | `OK -1`                  |
| `MUL -2 6`        | `OK -12`                 |
| `DIV 5 0`         | `ERR EZDV divisao_por_zero` |
| `5 + 5`           | `OK 10`                  |
| `10 / 4`          | `OK 2.5`                 |
| `SOMA 1 2`        | `ERR EINV entrada_invalida` |
---
