#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

int main() {
    // Initialisieren Sie die Datenbankverbindung
    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");
    db.setHostName("localhost");  // Setzen Sie Ihren Datenbankhost
    db.setDatabaseName("verbundprojekt");  // Setzen Sie Ihren Datenbanknamen
    db.setUserName("root");  // Setzen Sie Ihren Benutzernamen
    db.setPassword("konrad123");  // Setzen Sie Ihr Passwort
    
    // Verbindung zur Datenbank 
    if (!db.open()) {
        qDebug() << "Fehler beim Verbinden zur Datenbank: " << db.lastError().text();
        return 1;
    } else {
        qDebug() << "Erfolgreich mit der Datenbank verbunden.";
    }
    
    // JSON-Objekts für den productionStatus ( wird nachher aus den RFID Daten erstellt)
    QJsonObject productionStatus;
    productionStatus["raw"] = true;
    productionStatus["machine1"] = true;
    productionStatus["machine2"] = true;
    
    // Konvertieren des JSON-Objekts in einen String
    QJsonDocument doc(productionStatus);
    QString productionStatusStr = doc.toJson(QJsonDocument::Compact);
    

    QSqlQuery queryWorkpiece;
    queryWorkpiece.prepare("INSERT INTO workpieces (productionStatus) VALUES (:productionStatus)");
    queryWorkpiece.bindValue(":productionStatus", productionStatusStr);
    if (!queryWorkpiece.exec()) {
        qDebug() << "Fehler beim Einfügen in workpieces: " << queryWorkpiece.lastError().text();
        return 1;
    }
    
    qDebug() << "Neues Werkstück erfolgreich hinzugefügt. Mit WorkpieceID: " << queryWorkpiece.lastInsertId().toInt();
    
    int newWorkpieceId = queryWorkpiece.lastInsertId().toInt();
    
    // New order (wird nachher aus dem Produktkatalog ausgewählt)
    QSqlQuery queryNewOrder;
    // Erstellen des JSON-Objekts
    QJsonObject produktObjekt;
    produktObjekt["Produkt"] = "Produktname";
    
    QJsonArray fertigungsArray;
    QJsonObject station1;
    station1["Station"] = 1;
    station1["DauerInMinuten"] = 10;
    fertigungsArray.append(station1);
    
    QJsonObject station2;
    station2["Station"] = 2;
    station2["DauerInMinuten"] = 5;
    fertigungsArray.append(station2);
    
    QJsonObject station3;
    station3["Station"] = 3;
    station3["DauerInMinuten"] = 1;
    fertigungsArray.append(station3);
    
    produktObjekt["Fertigungsreihenfolge"] = fertigungsArray;
    

    queryNewOrder.prepare("INSERT INTO orders (productManufactSequence) VALUES (:sequence)");
    queryNewOrder.bindValue(":sequence", QJsonDocument(produktObjekt).toJson());
    if (!queryNewOrder.exec()) {
        qDebug() << "Fehler beim Einfügen in orders: " << queryNewOrder.lastError().text();
        return 1;
    }
    
    int orderId = queryNewOrder.lastInsertId().toInt();
    
    qDebug() << "Neue Order erfolgreich hinzugefügt. Mit orderID: " << orderId;
    
    // Sequence
    QSqlQuery queryOrder;
    queryOrder.prepare("SELECT productManufactSequence FROM orders WHERE idorders = :orderId");
    queryOrder.bindValue(":orderId", orderId);
    if (!queryOrder.exec()) {
        qDebug() << "Fehler beim Lesen von orders: " << queryOrder.lastError().text();
        return 1;
    }
    
    if (!queryOrder.next()) {
        qDebug() << "Order nicht gefunden.";
        return 1;
    }
    
    // Extrahieren des drivingJob aus der sequence
    QJsonDocument doc1 = QJsonDocument::fromJson(queryOrder.value(0).toByteArray());
    QJsonArray sequence = doc1.array();
    QString drivingJob;
    if (!sequence.isEmpty()) {
        // z. B. erster Eintrag in sequence ist der erste Fahrauftrag
        drivingJob = sequence[0].toString();
        // Driving Job wird dann per MQTT an Gewerk 2 verschickt im jeweiligen Zustand der zukünftigen StateMachine
        // drivingJob z. B. von raw zu machine1: zuerst ein drivingJob von rawTasche1 zum RFID Scan, danach con RFID Scan raw zu RFID Scan machine1 usw.
    }
    
    // // 4. Ermitteln Sie die ID des workpieces, das mit dieser Order verknüpft ist
    // QSqlQuery queryWorkpiece2;
    // queryWorkpiece2.prepare("SELECT workpieces_idworkpieces FROM workpieces_has_orders WHERE orders_idorders = :orderId");
    // queryWorkpiece2.bindValue(":orderId", orderId);
    // if (!queryWorkpiece2.exec()) {
    //     qDebug() << "Fehler beim Lesen von workpieces_has_orders: " << queryWorkpiece2.lastError().text();
    //     return 1;
    // }
    
    // if (!queryWorkpiece2.next()) {
    //     qDebug() << "Kein workpiece für diese Order gefunden.";
    //     return 1;
    // }
    // int workpieceId = queryWorkpiece2.value(0).toInt();
    
    // 5. Speichern Sie die Verknüpfung zwischen dem workpiece und dem drivingJob in 'workpieces_has_drivingJobs'
    // Fügen Sie einen neuen drivingJob in die Tabelle 'drivingJobs' ein
    
    QSqlQuery queryNewDrivingJob;
    queryNewDrivingJob.prepare("INSERT INTO drivingJobs (startpoint, targetpoint) VALUES (:startpoint, :targetpoint)");
    queryNewDrivingJob.bindValue(":startpoint", "StartPunktA");
    queryNewDrivingJob.bindValue(":targetpoint", "ZielPunktB");
    if (!queryNewDrivingJob.exec()) {
        qDebug() << "Fehler beim Einfügen in drivingJobs: " << queryNewDrivingJob.lastError().text();
        return 1;
    }
    
    int newDrivingJobId = queryNewDrivingJob.lastInsertId().toInt();
    //int drivingJobId = 1; // Setzen Sie hier die tatsächliche ID des drivingJob ein
    
    // Link workpiece und drivingJob
    QSqlQuery queryLink;
    queryLink.prepare("INSERT INTO workpieces_has_drivingJobs (workpieces_idworkpieces, drivingJobs_iddrivingJobs) VALUES (:workpieceId, :drivingJobId)");
    queryLink.bindValue(":workpieceId", newWorkpieceId);
    queryLink.bindValue(":drivingJobId", newDrivingJobId);
    if (!queryLink.exec()) {
        qDebug() << "Fehler beim Einfügen in workpieces_has_drivingJobs: " << queryLink.lastError().text();
        return 1;
    }
    
    qDebug() << "Verknüpfung zwischen workpiece und drivingJob erfolgreich erstellt.";
    
    // Link workpiece und prder
    QSqlQuery queryLink2;
    queryLink2.prepare("INSERT INTO workpieces_has_orders (workpieces_idworkpieces, orders_idorders) VALUES (:workpieceId, :orderId)");
    queryLink2.bindValue(":workpieceId", newWorkpieceId);
    queryLink2.bindValue(":orderId", orderId);
    if (!queryLink2.exec()) {
        qDebug() << "Fehler beim Einfügen in workpieces_has_orders: " << queryLink2.lastError().text();
        return 1;
    }
    
    return 0;
}
