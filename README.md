# Progetto Sistemi Operativi Avanzati: Multi-flow Device Driver

## Specifica
La specifica è relativa ad un driver Linux che implementa flussi di dati ad alta e bassa priorità. Attraverso una sessione aperta verso il device file, un thread può leggere e scrivere segmenti dati. La consegna dei dati segue una policy First-in-First-out lungo ciascuno dei due diversi flussi di dati (bassa e alta priorità).

Dopo le operazioni di lettura, i dati letti scompaiono dal flusso. Inoltre, il flusso dati ad alta priorità deve offrire operazioni di scrittura sincrone mentre il flusso dati a bassa priorità deve offrire un’esecuzione asincrona (basata su delayed work) delle operazioni di scrittura, pur mantenendo l’interfaccia in grado di notificare in modo sincrono l’esito. 
Le operazioni di lettura sono tutte eseguite in maniera sincrona. 

Il device driver dovrebbe supportare 128 devices corrispondenti alla stessa quantità di minor number.

Il device driver dovrebbe implementare il supporto per il servizio ioctl(..) in modo tale da gestire la sessione di I/O come segue:  
• Setup del livello di priorità (alto o basso) per le operazioni;  
• Operazioni di lettura e scrittura bloccanti vs non bloccanti;  
• Setup del timeout che regola il risveglio delle operazioni bloccanti.  

Alcuni parametri e funzioni del modulo Linux dovrebbero essere implementati in modo tale da poter abilitare o disabilitare un device file, in termini di specifico minor number.
Se è disabilitato, qualsiasi tentativo di aprire una sessione dovrebbe fallire (ma sessioni già aperte verranno comunque gestite). 

Ulteriori parametri esposti via VFS dovrebbero fornire un’immagine dello stato corrente del device in accordo alle seguenti informazioni:  
• Abilitato o disabilitato;  
• Numero di bytes correntemente presenti nei due flussi (alta e bassa priorità);  
• Numero di threads correntemente in attesa di dati lungo i due flussi (alta e bassa priorità).  
  

## Installazione
L’installazione del modulo viene realizzata dallo script `driver/install.bash`, al suo interno si effettua la compilazione ed il montaggio del modulo, gestendo anche il caso in cui il modulo sia già presente: viene rimosso insieme ai devices e nuovamente compilato per essere re-installato.

In alternativa, è comunque possibile effettuare la compilazione e l’installazione tramite i comandi manuali `make all` e `insmod multi_flow_device_driver.ko`, rimuovendo poi con `make clean` tutti i files prodotti dalla compilazione del modulo.

Quando l’installazione del modulo va a buon fine, l’esito viene riportato sul buffer del kernel e il driver viene registrato in `/proc/devices`, per cui il major number assegnato al driver può essere recuperato tramite:  
• Il comando `dmesg`.  
• `cat /proc/devices | grep multi_flow_device_driver | cut -d “ “ -f 1`.  
  
La rimozione del modulo viene effettuata tramite `rmmod multi_flow_device_driver.ko`.  


## Device Creation
Prima di poter utilizzare l'applicazione utente, è necessario creare dei dispositivi con cui interagire.

La creazione dei dispositivi viene realizzata dallo script `scripts/create_devices.bash`, si specifica come parametro il numero di device files da creare e se questo è valido, ossia tra 1 e 128, si creano iterativamente sfruttando il comando `mknod`: i dispositivi vengono creati dal percorso `/dev/multi_flow_device_*`, e al nome viene affiancato il corrispondente minor number.

Per rimuovere tutti i dispositivi, si fa uso del comando `rm dev/multi_flow_device_*`.  


## User CLI

L’interfaccia a riga di comando offerta all’utente viene eseguita tramite il programma `user/user`, che permette ad un client di interagire con dispositivi multi-flusso sfruttando il driver installato.

Il programma richiede in input un solo argomento da riga di comando, che corrisponde al percorso sul VFS di un certo device file esistente con il quale si vuole interagire: se il percorso fornito in input è valido, viene aperta una sessione verso il device file, solo a questo punto la CLI mette a disposizione dell’utente le diverse operazioni possibili su un certo dispositivo, come presentato nella specifica.

Le operazioni disponibili sono le seguenti:  
• SWITCH PRIORITY TYPE, modifica il parametro `priority` della sessione per poter lavorare sui due diversi flussi di dati disponibili.  
• SWITCH OPERATIONS TYPE, modifica il parametro `flags` della sessione per poter lavorare sui flussi di dati tramite operazioni bloccanti o meno.  
• SET TIMEOUT, modifica il parametro `timeout` della sessione per impostare il tempo massimo di attesa per prendere il lock sul flusso nelle operazioni bloccanti.  
• SWITCH DEVICE STATUS, modifica il parametro `enabled` del modulo per abilitare o disabilitare il dispositivo in uso.  
• WRITE, effettua la scrittura dei dati inseriti dall’utente su un preciso flusso legato al dispositivo in uso tramite la syscall `write()`.  
    ◦ Nel caso di scrittura a bassa priorità (asincrona), il client si mette in attesa del segnale di completamento dal modulo perchè si vuole mantenere l'interfaccia in grado di notificare l'output in maniera sincrona.  
• READ, effettua la lettura del numero di bytes specificati dall’utente da un preciso flusso legato al dispositivo in uso tramite la syscall `read()`.  
• QUIT, effettua la chiusura della sessione verso il dispositivo in uso tramite la syscall `close()` e termina l’esecuzione del programma.  
  
I comandi che permettono la modifica di parametri della sessione e del modulo utilizzano invece l’API di `ioctl`, offerta proprio per supportare operazioni non definite dal driver.
