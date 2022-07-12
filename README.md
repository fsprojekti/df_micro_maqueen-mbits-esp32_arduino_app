# Arduino Application for DFRobot micro:Maqueen Plus EN V1 Robot Car

## DFRobot micro:Maqueen Plus EN V1 Robot Car
An Advanced STEM Education Robot with six line-tracking sensors, IR received, ultrasonic sensor, RGB ambient lights, LED car lights, buzzer etc.
For more info check: https://www.dfrobot.com/product-2026.html

## Mbits ESP32 Development Board
A pocket-sized microcontroller that has lots of features the same as Microbit V2. Mbits is an ESP32-WROVER-B based educational hardware suitable for students to learn how to program and experience hot topics such as IoT and AI. It ntegrates input and output hardware such as 25 full-color LED lights, microphones, buzzers and it is also equipped with a 2.4GHz Radio/BLE WiFi & Bluetooth 4.2. The programming can be done in Arduino IDE.

## Robot Car Communication Diagram

[![](https://mermaid.ink/img/pako:eNqNksFqGzEQhl9l0MkGx5Qel2AITg-lpYEEfPJlLM3GKrsaVZoNmJB3z1haJW0CSXf3MNL8mvn-WT0ay45MZzL9mShYuvZ4n3DcBwCImMRbHzEIbAEz3PKBNcQEWw6SeICrGN9Jd_9Ki-QsKsJfLAT8QFqie6mCMQ7eongOkMiSf6CsgRJlydBzAkkYck8pr-HykDbfBXTD8TicwB6Zs-rlSJBKV6td5YjyeqokldHSsC4Y2kVL3B9w8WUF9Vt_XZaUPtuLzWbXwaiciyxq7SdXuhVQcG1R5brxzpqevX0hQWspSkVoQM0c-L74Aa-hOk1EAGpM87-VMJf9fs4epnz6D_pmYqcmdMbnU7Bo_Wpdcss3qpsfr5oK3DSf-jtPSd1x_QFawydyMMxDqsCt28djn1nakLY8xoEU5G-O-pqVGSmN6J1e3cdyuYy2H2lvOg0d9TgNsjf78KTSKToU-ua8cDJdj0OmlcFJ-O4UrOkkTdRE8_WfVU_PvfIFSg)](https://mermaid-js.github.io/mermaid-live-editor/edit#pako:eNqNksFqGzEQhl9l0MkGx5Qel2AITg-lpYEEfPJlLM3GKrsaVZoNmJB3z1haJW0CSXf3MNL8mvn-WT0ay45MZzL9mShYuvZ4n3DcBwCImMRbHzEIbAEz3PKBNcQEWw6SeICrGN9Jd_9Ki-QsKsJfLAT8QFqie6mCMQ7eongOkMiSf6CsgRJlydBzAkkYck8pr-HykDbfBXTD8TicwB6Zs-rlSJBKV6td5YjyeqokldHSsC4Y2kVL3B9w8WUF9Vt_XZaUPtuLzWbXwaiciyxq7SdXuhVQcG1R5brxzpqevX0hQWspSkVoQM0c-L74Aa-hOk1EAGpM87-VMJf9fs4epnz6D_pmYqcmdMbnU7Bo_Wpdcss3qpsfr5oK3DSf-jtPSd1x_QFawydyMMxDqsCt28djn1nakLY8xoEU5G-O-pqVGSmN6J1e3cdyuYy2H2lvOg0d9TgNsjf78KTSKToU-ua8cDJdj0OmlcFJ-O4UrOkkTdRE8_WfVU_PvfIFSg)

'''mermaid
sequenceDiagram
   participant C as Robot Car Control App
   participant V as Robot Car App

  
   Note over C: Control application receives requests for transfers. <br>It randomly chooses the robot car that transfers the parcel.
   rect rgba(0, 0, 0, .2)
      C->>V: move(startLocation, endLocation)
   end  
   Note over V: Robot car accepts the transfer request if <br> it is free  and rejects it if it is busy.
   rect rgba(0, 0, 0, .2)
     
      V->>C: busy (request rejected)
      V->>C: OK (request accepted)
   end  
   Note over V: Robot car moves to the required location.
   
      rect rgba(0, 0, 0, .2)
      V->>C: transferCompleted
   end  

'''