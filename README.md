# Real-Time Typing Speed System with STM32 and FreeRTOS

This system is designed using a real-time operating system (RTOS)-based structure operating over UART communication between a Python-based GUI and the STM32F407VGT microcontroller. The system’s operation process is organized according to the FreeRTOS task structure.

## Game Start

- The user presses the **"Start Test"** button on the Python GUI.  
- Python sends the **"begin"** command to the STM32 via UART communication.  
- Inside the STM32, the **CountdownTask** is triggered by a semaphore (**startSem**) that waits for this command, and the game starts.

## Countdown and Game Process

- When the game starts, **CountdownTask** begins a countdown from 30 seconds on the 7-segment display.  
- Simultaneously, the OLED screen displays the message **"Start!"**  
- During the countdown, simultaneous operations are performed on both the OLED and 7-segment displays. Access to these resources is secured using a FreeRTOS mutex (**i2cMutex**).  
- The player tries to type the words shown on the Python GUI correctly and completely within the allotted time.

## Test End and Displaying Results

- When the time runs out and the countdown reaches zero, **CountdownTask** informs the **DisplayTask** via the semaphore (**finishSem**).  
- The Python GUI sends the number of correctly typed words to the STM32 via UART.  
- **DisplayTask** receives this word count and writes the result on the OLED screen in the format:  
  `True Words: X`  
- During operations on the OLED and 7-segment display, resource sharing is again protected with a mutex.

---

## System Images

<p align="center">
  <img src="https://github.com/user-attachments/assets/f4135eec-7c5e-458c-98b5-32a4f0421473" alt="STM32F407G-DISC1" width="400"/>
  <br><em>STM32F407G-DISC1 Board</em>
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/0c077715-1354-4f70-b179-34d7c457ccec" alt="Board Connection" width="400"/>
  <br><em>Connection with the Breadboard</em>
</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/f8937efa-a5ae-46ba-b25f-8d12a91e1096" alt="Flowchart" width="600"/>
  <br><em>Flowchart of the System</em>
</p>
