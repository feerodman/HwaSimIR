void MainWindow::setupSensorForm(QVBoxLayout* mainLayout)
{
    auto* group=new QGroupBox(QString::fromUtf8("初始化 · 图像、安装与既有照明参数"));
    group->setObjectName("sensorInitForm");auto* grid=new QGridLayout(group);
    grid->setContentsMargins(8,12,8,4);grid->setHorizontalSpacing(10);grid->setVerticalSpacing(2);
    QSettings settings(m_networkConfigPath,QSettings::IniFormat);settings.setIniCodec("UTF-8");
    for(int i=0;i<HwaSensorFields::count;++i){
        const auto& field=HwaSensorFields::fields()[i];QWidget* control=nullptr;
        if(field.kind==HwaSensorFields::Boolean){auto* box=new QCheckBox;box->setChecked(field.initial!=0);control=box;}
        else if(field.kind==HwaSensorFields::Band){
            auto* box=new QComboBox;
            box->addItem(QString::fromUtf8("0 · 短波 SWIR"),0);box->addItem(QString::fromUtf8("近红外（NIR）"),1);
            box->addItem(QString::fromUtf8("2 · 中波 MWIR"),2);box->addItem(QString::fromUtf8("3 · 长波 LWIR"),3);
            box->addItem(QString::fromUtf8("4 · 可见光"),4);control=box;
        }else if(field.kind==HwaSensorFields::Integer){auto* box=new QSpinBox;box->setRange(static_cast<int>(field.minimum),static_cast<int>(field.maximum));control=box;}
        else {auto* box=new QDoubleSpinBox;box->setDecimals(field.decimals);box->setRange(field.minimum,field.maximum);box->setSingleStep(.1);control=box;}
        control->setObjectName(QString::fromLatin1(field.name));control->setMinimumWidth(90);
        control->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        control->setToolTip(QString::fromLatin1(field.name)+"\n"+QString::fromUtf8(field.unit));
        m_sensorControls.insert(QString::fromLatin1(field.name),control);
        const double value=settings.value("SensorInit/"+QString::fromLatin1(field.name),field.initial).toDouble();
        if(!setSensorField(QString::fromLatin1(field.name),value))qFatal("Invalid SensorInit field: %s",field.name);
        const int row=i/3,col=(i%3)*2;
        const QString unit=QString::fromUtf8(field.unit);
        grid->addWidget(new QLabel(QString::fromUtf8(field.label)+(unit.isEmpty()?QString():" ("+unit+")")),row,col);
        grid->addWidget(control,row,col+1);
    }
    m_simModeBox=new QComboBox;m_simModeBox->setObjectName("simMode");
    m_simModeBox->addItem(QString::fromUtf8("同步 (1)"),1);m_simModeBox->addItem(QString::fromUtf8("异步 (2)"),2);
    m_simModeBox->setCurrentIndex(m_simModeBox->findData(m_protocolSimMode));
    m_videoFpsEdit->setObjectName("videoFps");m_videoFpsEdit->setValidator(new QIntValidator(0,240,m_videoFpsEdit));
    grid->addWidget(new QLabel(QStringLiteral("simMode")),9,0);grid->addWidget(m_simModeBox,9,1);
    grid->addWidget(new QLabel(QString::fromUtf8("videoFps (0 不限)")),9,2);grid->addWidget(m_videoFpsEdit,9,3);
    for(int c=1;c<6;c+=2)grid->setColumnStretch(c,1);
    mainLayout->addWidget(group);
}

bool MainWindow::setSensorField(const QString& name,double value)
{
    const auto* control=m_sensorControls.value(name,nullptr);if(!control)return false;
    for(int i=0;i<HwaSensorFields::count;++i){const auto& field=HwaSensorFields::fields()[i];
        if(name!=QString::fromLatin1(field.name))continue;
        BYHWICD::trackerSensorParam probe={};if(!HwaSensorFields::set(probe,field,value))return false;
        auto* widget=m_sensorControls.value(name);
        if(auto* box=qobject_cast<QCheckBox*>(widget))box->setChecked(value!=0);
        else if(auto* box=qobject_cast<QComboBox*>(widget))box->setCurrentIndex(box->findData(static_cast<int>(value)));
        else if(auto* box=qobject_cast<QSpinBox*>(widget))box->setValue(static_cast<int>(value));
        else if(auto* box=qobject_cast<QDoubleSpinBox*>(widget))box->setValue(value);
        return true;
    }return false;
}

bool MainWindow::sensorFormSnapshot(BYHWICD::trackerSensorParam& sensor,QString& error) const
{
    sensor={};
    for(int i=0;i<HwaSensorFields::count;++i){const auto& field=HwaSensorFields::fields()[i];
        auto* widget=m_sensorControls.value(QString::fromLatin1(field.name));double value=0;
        if(auto* box=qobject_cast<QCheckBox*>(widget))value=box->isChecked()?1:0;
        else if(auto* box=qobject_cast<QComboBox*>(widget))value=box->currentData().toInt();
        else if(auto* box=qobject_cast<QSpinBox*>(widget))value=box->value();
        else if(auto* box=qobject_cast<QDoubleSpinBox*>(widget))value=box->value();
        if(!HwaSensorFields::set(sensor,field,value)){error=QString::fromLatin1(field.name);return false;}
    }
    if(sensor.trackerSensorViewMax<=sensor.trackerSensorViewMin){error="viewMax must exceed viewMin";return false;}
    return true;
}

void MainWindow::setSendStepMs(double milliseconds)
{
    if(!std::isfinite(milliseconds)||milliseconds<.1||milliseconds>100000)qFatal("Invalid send step");
    m_timeStep->setText(QString::number(milliseconds,'f',6));
    m_sendStepMs=milliseconds;m_inputHz=1000.0/milliseconds;time_step=milliseconds;
}
