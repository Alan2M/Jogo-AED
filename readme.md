# 🌍🔥💧 Elements

**Elements** é uma releitura do clássico *Fireboy and Watergirl*, adicionando um terceiro personagem: o **Earthboy** 🌱.
Desenvolvido em **C** com a biblioteca **Raylib**, o jogo combina plataforma, lógica e cooperação, utilizando **árvores binárias** para representar o progresso das fases.

---

## 🎮 Sobre o Jogo

O *Elements* é um jogo cooperativo para **três jogadores**, cada um controlando um personagem elemental:

* 🔥 **Fireboy** — sobrevive apenas em áreas de fogo
* 💧 **Watergirl** — sobrevive apenas em áreas de água
* 🌱 **Earthboy** — sobrevive apenas em áreas de terra

Se qualquer personagem tocar um elemento que não seja o seu, ele morre instantaneamente, exigindo **sincronização e trabalho em equipe**.

Após completar uma fase, o tempo da equipe é registrado e organizado em um ranking através do algoritmo **Insertion Sort**.

---

## 👥 Equipe de Desenvolvimento

* Alan Matos
* Gabriela Monteiro
* João Guilherme Omena
* Nathália Carneiro

---

## 🕹️ Personagens e Controles

| Personagem       | Teclas  | Elemento | Características                               |
| ---------------- | ------- | -------- | --------------------------------------------- |
| 🔥 **Fireboy**   | ← → ↑   | Fogo     | Resistente ao fogo; vulnerável à água e terra |
| 💧 **Watergirl** | W, A, D | Água     | Resistente à água; vulnerável ao fogo e terra |
| 🌱 **Earthboy**  | I, J, L | Terra    | Resistente à terra; vulnerável ao fogo e água |

> ⚠️ **Atenção:** Todos os personagens só podem interagir com o seu próprio elemento.

---

## 🌳 Estrutura de Dados

As fases são organizadas em uma **árvore binária**, onde cada nó é uma fase e cada ramo representa caminhos alternativos.

### Distribuição das Fases

* 🌱 **2 fases fáceis**
* 🔥 **2 fases médias**
* 💧 **1 fase difícil**

---

## 🏆 Sistema de Ranking

O tempo de cada fase é registrado e os resultados são ordenados usando **Insertion Sort**, exibindo do menor para o maior tempo.

---

## ⚙️ Tecnologias Utilizadas

* **Linguagem:** C
* **Biblioteca gráfica:** Raylib
* **Estrutura de dados:** Árvores Binárias
* **Algoritmo de ordenação:** Insertion Sort
* **Paradigma:** Programação Estruturada

---

## 🚀 Como Jogar

Compile o projeto:

```bash
make clean
make
make run
```

---

## 🎥 Vídeo Demonstrativo

[https://youtu.be/ZpZ_mXSi1Hc](https://youtu.be/ZpZ_mXSi1Hc)
