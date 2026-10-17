#pragma once
#include "ICD/common_data.h"
#include "ReplayCsvSchema.h"
#include "../Shared/NumericCsvReader.h"
#include <QFile>
#include <QFileInfo>
#include <QVector>

// 普通输入仅按格式、数值与时间校验；不按名字、SHA 或历史行数准入。
inline bool loadReplaySource(const QString& path, QVector<ICD::realtimeInfo>& output, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { error = path + ": " + file.errorString(); return false; }
    const auto parsed = NumericCsv::read(QString::fromUtf8(file.readAll()), replayCsvSchema(), {17,28,29,53,54,55,62});
    if (!parsed.ok()) {
        const auto& e = parsed.errors.first();
        error = QString("第 %1 行 / %2：%3").arg(e.sourceLine).arg(e.field).arg(e.reason);
        return false;
    }
    QVector<ICD::realtimeInfo> rows;
    for (const auto& row : parsed.rows) {
        const auto& f = row.fields;
        ICD::realtimeInfo d;
        d.sourceLine=row.sourceLine; d.sourceTimeMs=f[0].toDouble(); d.distance=f[1].toDouble();
        d.platPos.lat=f[2].toDouble(); d.platPos.lon=f[3].toDouble(); d.platPos.alt=f[4].toDouble();
        d.platEul.yaw=f[5].toDouble(); d.platEul.pitch=f[6].toDouble(); d.platEul.roll=f[7].toDouble(); d.platSpeed=f[8].toDouble();
        d.tarPos.lat=f[10].toDouble(); d.tarPos.lon=f[11].toDouble(); d.tarPos.alt=f[12].toDouble();
        d.tarEul.yaw=f[13].toDouble(); d.tarEul.pitch=f[14].toDouble(); d.tarEul.roll=f[15].toDouble(); d.tarSpeed=f[16].toDouble();
        if (f[17].toInt()!=0 && f[17].toInt()!=1) { error=QString("第 %1 行 MissileType 只支持 0/1").arg(row.sourceLine); return false; }
        d.targetType=f[17].toInt()==0?0x22:0x33;
        d.strikeFlag=f[28].toInt()!=0; d.damageFlag=f[29].toInt()!=0; d.viewValid=f[53].toInt()!=0;
        if (!rows.isEmpty() && d.sourceTimeMs<=rows.last().sourceTimeMs) { error=QString("第 %1 行 Time(ms) 必须递增").arg(row.sourceLine); return false; }
        rows.push_back(d);
    }
    if (rows.isEmpty()) { error="输入没有有效数据行"; return false; }
    output.swap(rows);
    return true;
}
