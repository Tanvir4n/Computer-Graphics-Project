# 🏫 Smart Primary School Environment (OpenGL)

## 📌 Overview
This project is a **2D animated smart city environment** built using **OpenGL (GLUT)**. It simulates a primary school surroundings with dynamic elements like vehicles, weather, day/night modes, and environmental objects.

The project demonstrates fundamental **computer graphics algorithms** such as:
- DDA Line Algorithm  
- Bresenham’s Line Algorithm  
- Midpoint Circle Algorithm  

---

## 🎯 Features

### 🌤️ Environment
- Day and Night mode
- Gradient sky
- Sun (day) and Moon + Stars (night)

### 🌧️ Weather System
- Rain animation with falling drops and splash effect
- Dynamic cloud movement

### 🚗 Traffic System
- Multiple moving cars
- Police car
- School bus
- Road with lane markings and zebra crossing

### 🏢 Buildings
- School building
- Bank
- Police station
- Random buildings

### 🌳 Natural Elements
- Trees with trunks and leaves
- Grass field
- Clouds and birds animation

### 🚦 Smart Elements
- Traffic light system
- Street lights (active in night mode)

---

## 🧠 Algorithms Used

### 1. DDA Line Algorithm
Used for:
- Bird wings  
- Rain drops  
- Tree trunks  
- Street light poles  

### 2. Bresenham’s Line Algorithm
Used for:
- Road lane markings  
- Zebra crossing  
- Structural outlines  

### 3. Midpoint Circle Algorithm
Used for:
- Sun and Moon  
- Car wheels  
- Traffic lights  
- Tree leaves  
- Clouds  

---

## 🎮 Controls

| Key | Action |
|-----|--------|
| `r` | Start Rain |
| `s` | Stop Rain |
| `n` | Night Mode |
| `d` | Day Mode |

---

## ⚙️ Installation & Setup

### Requirements
- C++ Compiler (GCC / MinGW / Visual Studio)
- OpenGL
- GLUT Library

### Compile & Run (Linux / Mac)
```bash
g++ main.cpp -o project -lGL -lGLU -lglut
./project