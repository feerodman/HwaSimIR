// Kept separate from transport so this presentation change can be rolled back
// without changing product identity, recording, or rendering.
void HwaSim_IR_VideoDisplay::setupResponsiveLayout()
{
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"),9));
    auto* page=new QWidget;page->setObjectName("telemetryPage");
    auto* column=new QVBoxLayout(page);column->setContentsMargins(6,4,6,4);column->setSpacing(2);
    auto* monitor=new QGroupBox(QString::fromUtf8("运行监测"),page);
    auto* metrics=new QGridLayout(monitor);metrics->setContentsMargins(5,7,5,2);metrics->setSpacing(4);
    const QString names[]={QString::fromUtf8("视频 FPS"),QString::fromUtf8("数据接收 Hz"),QString::fromUtf8("指令处理 Hz"),QString::fromUtf8("输出延时")};
    for(int i=0;i<4;++i){
        m_metricLabels[i]=new QLabel(names[i]+QString::fromUtf8("  —"),monitor);
        m_metricLabels[i]->setObjectName(QString("runtimeMetric%1").arg(i));
        m_metricLabels[i]->setMinimumWidth(0);m_metricLabels[i]->setWordWrap(true);
        metrics->addWidget(m_metricLabels[i],i/2,i%2);
    }
    column->addWidget(monitor);
    auto* env=new QGroupBox(QString::fromUtf8("场景与天气"),page);auto* envGrid=new QGridLayout(env);
    envGrid->setContentsMargins(5,7,5,2);envGrid->setSpacing(2);
    struct Entry{const char* label;QLineEdit* edit;};
    const Entry environment[]={
        {"地形",ui.lineEdit_sceneType},{"天气",ui.lineEdit_envSky},{"状态",ui.lineEdit_controlType},
        {"雨层上限 m",ui.lineEdit_envMaxHeightRain},{"雨层过渡 m",ui.lineEdit_envTransHeightRain},{"相对速度",ui.lineEdit_envRainSnowSpeedScale},
        {"雪层上限 m",ui.lineEdit_envMaxHeightSnow},{"雪层过渡 m",ui.lineEdit_envTransHeightSnow},{"能见度 m",ui.lineEdit_envVisibility},
        {"环境温度 °C",ui.lineEdit_envTemp},{"湿度 %",ui.lineEdit_envHumidity},{"风速 m/s",ui.lineEdit_envWindV},
        {"地形倍率",ui.lineEdit_envRadScaleTerrain},{"天空倍率",ui.lineEdit_envRadScaleSky},{"风向 °",ui.lineEdit_envWindDir}};
    int k=0;for(const auto& e:environment){
        e.edit->setParent(env);e.edit->setMinimumWidth(0);e.edit->setMaximumWidth(QWIDGETSIZE_MAX);
        e.edit->setReadOnly(true);e.edit->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
        envGrid->addWidget(new QLabel(QString::fromUtf8(e.label),env),k/4,(k%4)*2);
        envGrid->addWidget(e.edit,k/4,(k%4)*2+1);++k;
    }column->addWidget(env);
    auto* sensor=new QGroupBox(QString::fromUtf8("传感器初始化"),page);auto* sensorGrid=new QGridLayout(sensor);
    sensorGrid->setContentsMargins(5,7,5,2);sensorGrid->setSpacing(2);
    for(int i=0;i<HwaSensorFields::count;++i){
        const auto& field=HwaSensorFields::fields()[i];
        auto* edit=new QLineEdit(sensor);edit->setObjectName(QString::fromLatin1(field.name));
        edit->setReadOnly(true);edit->setMinimumWidth(0);edit->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
        m_sensorFields[i]=edit;
        const QString unit=(i==2||i==5||i==24||i==26)?QString():QString::fromUtf8(field.unit);
        auto* label=new QLabel(QString::fromUtf8(field.label)+(unit.isEmpty()?QString():" "+unit),sensor);
        label->setToolTip(QString::fromLatin1(field.name)+" / "+QString::fromUtf8(field.unit));
        sensorGrid->addWidget(label,i/4,(i%4)*2);sensorGrid->addWidget(edit,i/4,(i%4)*2+1);
    }
    m_simModeDisplay=new QLineEdit(sensor);m_simModeDisplay->setReadOnly(true);m_simModeDisplay->setMinimumWidth(0);
    sensorGrid->addWidget(new QLabel(QString::fromUtf8("仿真模式"),sensor),7,0);sensorGrid->addWidget(m_simModeDisplay,7,1);
    ui.lineEdit_videoFps->setParent(sensor);ui.lineEdit_videoFps->setMinimumWidth(0);
    sensorGrid->addWidget(new QLabel(QString::fromUtf8("视频 FPS"),sensor),7,2);sensorGrid->addWidget(ui.lineEdit_videoFps,7,3);
    for(int c=1;c<8;c+=2){envGrid->setColumnStretch(c,1);sensorGrid->setColumnStretch(c,1);}
    column->addWidget(sensor);
    for(auto* group:{ui.groupBox_platData,ui.groupBox_targetData}){
        group->setParent(page);group->setMinimumSize(0,0);group->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
        group->layout()->setContentsMargins(4,7,4,2);column->addWidget(group);
    }
    column->addStretch(1);
    // A page scroll is available only below the documented full-screen envelope.
    // Platform and target tables themselves never hide columns behind scrollbars.
    auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumSize(0,0);scroll->setWidget(page);
    ui.dockWidget_dataShow->setWidget(scroll);
    ui.dockWidget_dataShow->setTitleBarWidget(new QWidget);
    ui.dockWidget_dataShow->setFeatures(QDockWidget::NoDockWidgetFeatures);
    ui.dockWidget_dataShow->setMinimumSize(0,0);ui.dockWidget_dataShow->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
    ui.widget_video->setMinimumSize(0,0);ui.widget_video->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
    ui.horizontalLayout->setContentsMargins(0,0,0,0);ui.horizontalLayout->setSpacing(8);
    ui.horizontalLayout->setStretch(0,1);ui.horizontalLayout->setStretch(1,1);
    ui.verticalLayout_2->setStretch(0,0);ui.verticalLayout_2->setStretch(1,1);
}

void HwaSim_IR_VideoDisplay::fitTelemetryTables()
{
    for(auto* table:{ui.tableWidget_platData,ui.tableWidget_targetData}){
        if(table->columnCount()==0)continue;
        const QFontMetrics fm(table->font());QVector<int> widths;int required=0;
        for(int c=0;c<table->columnCount();++c){
            int w=0;
            for(const auto& line:table->horizontalHeaderItem(c)->text().split('\n'))w=qMax(w,fm.horizontalAdvance(line));
            for(int r=0;r<table->rowCount();++r)if(table->item(r,c))w=qMax(w,fm.horizontalAdvance(table->item(r,c)->text()));
            widths.push_back(w+12);required+=w+12;
        }
        int extra=qMax(0,table->viewport()->width()-required);
        for(int c=0;c<widths.size();++c){const int share=extra/(widths.size()-c);table->setColumnWidth(c,widths[c]+share);extra-=share;}
        const int rowHeight=qMax(17,fm.height()+3);
        table->verticalHeader()->setMinimumSectionSize(rowHeight);
        table->verticalHeader()->setDefaultSectionSize(rowHeight);
        for(int r=0;r<table->rowCount();++r)table->setRowHeight(r,rowHeight);
        table->horizontalHeader()->setFixedHeight(fm.height()*2+4);
        table->setFixedHeight(table->rowCount()*rowHeight+table->horizontalHeader()->height()+2*table->frameWidth());
        if(required>table->viewport()->width())qWarning()<<"[UiLayout] tableFieldsDoNotFit"<<table->objectName()<<required<<table->viewport()->width();
    }
}

void HwaSim_IR_VideoDisplay::captureResponsiveUi(int)
{
    const QString base=QString::fromLocal8Bit(qgetenv("P6ReceiverUiDump"));
    auto* screen=windowHandle()?windowHandle()->screen():QGuiApplication::primaryScreen();
    QJsonObject report;report.insert("widgetWidth",width());report.insert("widgetHeight",height());
    report.insert("fullScreen",isFullScreen());report.insert("maximized",isMaximized());
    report.insert("devicePixelRatio",devicePixelRatioF());report.insert("leftWidth",ui.dockWidget_dataShow->width());
    report.insert("rightWidth",ui.widget_video->width());report.insert("videoPhysicalWidth",ui.m_Label_Video->width()*devicePixelRatioF());
    report.insert("videoPhysicalHeight",ui.m_Label_Video->height()*devicePixelRatioF());
    report.insert("sourceWidth",m_maxImageWidth);report.insert("sourceHeight",m_maxImageHeight);
    if(screen){report.insert("screenLogicalWidth",screen->geometry().width());report.insert("screenLogicalHeight",screen->geometry().height());report.insert("logicalDpi",screen->logicalDotsPerInch());}
    report.insert("captureKind","actual_widget_on_current_screen");
    for(auto* table:{ui.tableWidget_platData,ui.tableWidget_targetData}){
        QJsonObject item;item.insert("rows",table->rowCount());item.insert("columns",table->columnCount());
        item.insert("horizontalRange",table->horizontalScrollBar()->maximum());item.insert("verticalRange",table->verticalScrollBar()->maximum());
        auto* scroll=qobject_cast<QScrollArea*>(ui.dockWidget_dataShow->widget());
        const QRect onPage(table->mapTo(scroll->viewport(),QPoint(0,0)),table->size());
        const bool visible=scroll->viewport()->rect().contains(onPage);
        item.insert("entireTableVisible",visible);
        item.insert("pageScrollRange",scroll->verticalScrollBar()->maximum());
        item.insert("topOnPage",onPage.top());item.insert("bottomOnPage",onPage.bottom());
        bool fits=true;const QFontMetrics fm(table->font());
        for(int r=0;r<table->rowCount();++r)for(int c=0;c<table->columnCount();++c)if(table->item(r,c))
            fits=fits&&fm.horizontalAdvance(table->item(r,c)->text())+8<=table->columnWidth(c)&&table->visualItemRect(table->item(r,c)).bottom()<table->viewport()->height();
        item.insert("allCellTextFits",fits);report.insert(table->objectName(),item);
    }
    QFile file(base+".layout.json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(report).toJson());
    qInfo().noquote()<<"[P7UiLayout]"<<QJsonDocument(report).toJson(QJsonDocument::Compact);
}
