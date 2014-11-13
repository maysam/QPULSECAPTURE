#include <QXmlStreamReader>
#include <QXmlStreamAttributes>
#include <QFile>
#include "qharmonicprocessor.h"

//----------------------------------------------------------------------------------------------------------
QHarmonicProcessor::QHarmonicProcessor(QObject *parent, quint16 length_of_data, quint16 length_of_buffer) :
    QObject(parent),
    m_DataLength(length_of_data),
    m_BufferLength(length_of_buffer),
    curpos(0),
    m_SNR(-5.0),
    m_HeartRate(0.0),
    m_MeanCh1(0.0),
    m_MeanCh2(0.0),
    f_PCA(false),
    m_channel(Green),
    m_zerocrossing(0),
    m_PulseCounter(4),
    m_leftThreshold(70),
    m_rightTreshold(80),
    m_output(1.0),
    m_ID(0)
{
    // Memory allocation
    v_RawCh1 = new qreal[m_DataLength];
    v_RawCh2 = new qreal[m_DataLength];
    v_Signal = new qreal[m_DataLength];
    v_Time = new qreal[m_DataLength];
    v_Input = new qreal[DIGITAL_FILTER_LENGTH];
    v_ForFFT = new qreal[m_BufferLength];
    v_Spectrum = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * (m_BufferLength/2 + 1));
    v_Amplitude = new qreal[m_BufferLength/2 + 1];
    m_plan = fftw_plan_dft_r2c_1d(m_BufferLength, v_ForFFT, v_Spectrum, FFTW_ESTIMATE);
    v_BinaryOutput = new qreal[m_DataLength];
    v_SmoothedSignal = new qreal[DIGITAL_FILTER_LENGTH];

    // Vectors initialization
    for (quint16 i = 0; i < m_DataLength; i++)
    {
        v_RawCh1[i] = 0.0; // it should be equal to zero at start
        v_RawCh2[i] = 0.0; // it should be equal to zero at start
        v_Time[i] = 35.0; // just for ensure that at the begining there is not any "division by zero"
        v_Signal[i] = 0.0;
        if(i % 4)
        {
            v_BinaryOutput[i] = 1.0;
        }
        else
        {
            v_BinaryOutput[i] = -1.0;
        }
    }

    // Memory allocation block for ALGLIB arrays
    PCA_RAW_RGB.setlength(m_BufferLength, 3); // 3 because RED, GREEN and BLUE colors represent 3 independent variables
    PCA_Variance.setlength(3);
    PCA_Basis.setlength(3, 3);
    PCA_Info = 0;
}

//----------------------------------------------------------------------------------------------------------

QHarmonicProcessor::~QHarmonicProcessor()
{
    fftw_destroy_plan(m_plan);
    delete[] v_RawCh1;
    delete[] v_RawCh2;
    delete[] v_Signal;
    delete[] v_Time;
    delete[] v_Input;
    delete[] v_ForFFT;
    fftw_free(v_Spectrum);
    delete[] v_Amplitude;
    delete[] v_BinaryOutput;
    delete[] v_SmoothedSignal;
}

//----------------------------------------------------------------------------------------------------------

void QHarmonicProcessor::EnrollData(unsigned long red, unsigned long green, unsigned long blue, unsigned long area, double time)
{
    quint16 position = loopBuffer(curpos);
    PCA_RAW_RGB(position, 0) = (qreal)red / area;
    PCA_RAW_RGB(position, 1) = (qreal)green / area;
    PCA_RAW_RGB(position, 2) = (qreal)blue / area;

    if(m_channel == QHarmonicProcessor::All) {

        qreal ch1_temp = (qreal)(red - green) / area;
        qreal ch2_temp = (qreal)(red + green - 2 * blue) / area;

        m_MeanCh1 += (ch1_temp - v_RawCh1[curpos]) / m_DataLength;
        m_MeanCh2 += (ch2_temp - v_RawCh2[curpos]) / m_DataLength;
        v_RawCh1[curpos] = ch1_temp;
        v_RawCh2[curpos] = ch2_temp;

        qreal ch1_sko = 0.0;
        qreal ch2_sko = 0.0;
        for (unsigned int i = 0; i < m_DataLength; i++)
        {
            ch1_sko += (v_RawCh1[i] - m_MeanCh1)*(v_RawCh1[i] - m_MeanCh1);
            ch2_sko += (v_RawCh2[i] - m_MeanCh2)*(v_RawCh2[i] - m_MeanCh2);
        }
        ch1_sko = sqrt(ch1_sko / (m_DataLength - 1));
        ch2_sko = sqrt(ch2_sko / (m_DataLength - 1));
        v_Input[loopInput(curpos)] = (v_RawCh1[curpos] - m_MeanCh1) / ch1_sko  - (v_RawCh2[curpos] - m_MeanCh2) / ch2_sko;

    } else {

        qreal temp = 0.0;
        switch(m_channel) {
            case Red:
                temp = (qreal)red / area;
                break;
            case Green:
                temp = (qreal)green / area;
                break;
            case Blue:
                temp = (qreal)blue / area;
                break;
        }
        m_MeanCh1 += (temp - v_RawCh1[curpos])/m_DataLength;
        v_RawCh1[curpos] = temp;

        qreal ch1_sko = 0.0;
        for (unsigned int i = 0; i < m_DataLength; i++)
        {
            ch1_sko += (v_RawCh1[i] - m_MeanCh1)*(v_RawCh1[i] - m_MeanCh1);
        }
        ch1_sko = sqrt(ch1_sko / (m_DataLength - 1));
        v_Input[loopInput(curpos)] = (v_RawCh1[curpos] - m_MeanCh1)/ ch1_sko;

    }

    v_Time[curpos] = time;
    emit TimeUpdated(v_Time, m_DataLength);
    v_Signal[curpos] = ( v_Input[loopInput(curpos)] + v_Signal[loop(curpos - 1)] ) / 2.0;
    emit SignalUpdated(v_Signal, m_DataLength);

    //----------------------------------------------------------------------------
    qreal outputValue = 0.0;
    for(quint16 i = 0; i < DIGITAL_FILTER_LENGTH ; i++)
    {
        outputValue += v_Input[i];
    }
    v_SmoothedSignal[loopInput(curpos)] = outputValue / DIGITAL_FILTER_LENGTH;
    v_Derivative[loopOnTwo(curpos)] = v_SmoothedSignal[loopInput(curpos)] - v_SmoothedSignal[loopInput(curpos - (DIGITAL_FILTER_LENGTH - 1))];
    if( (v_Derivative[0]*v_Derivative[1]) < 0.0 )
    {
        m_zerocrossing = (++m_zerocrossing) % 2;
        if(m_zerocrossing == 0)
        {
            m_output *= -1.0;
        }
    }
    v_BinaryOutput[curpos] = m_output; // note, however, that v_BinaryOutput accumulate phase delay about DIGITAL_FILTER_LENGTH
    emit BinaryOutputUpdated(v_BinaryOutput, m_DataLength);
    //----------------------------------------------------------------------------

    emit CurrentValues(v_Signal[curpos], PCA_RAW_RGB(position, 0), PCA_RAW_RGB(position, 1), PCA_RAW_RGB(position, 2), m_HeartRate, m_SNR);
    curpos = (++curpos) % m_DataLength; // for loop-like usage of ptData and the other arrays in this class
}

//----------------------------------------------------------------------------------------------------------

void QHarmonicProcessor::ComputeFrequency()
{
    qint16 temp_position = curpos - 1;
    qreal buffer_duration = 0.0; // for buffer duration accumulation without first time interval
    if(f_PCA)
    {
        alglib::pcabuildbasis(PCA_RAW_RGB, m_BufferLength, 3, PCA_Info, PCA_Variance, PCA_Basis);
        if (PCA_Info == 1)
        {
            qreal mean0 = 0.0;
            qreal mean1 = 0.0;
            qreal mean2 = 0.0;
            for (unsigned int i = 0; i < m_BufferLength; i++)
            {
                mean0 += PCA_RAW_RGB(i,0);
                mean1 += PCA_RAW_RGB(i,1);
                mean2 += PCA_RAW_RGB(i,2);
            }
            mean0 /= m_BufferLength;
            mean1 /= m_BufferLength;
            mean2 /= m_BufferLength;

            qreal temp_sko = sqrt(PCA_Variance(0));
            qint16 start = loopBuffer(temp_position) - (m_BufferLength - 1); // has to be signed
            for (quint16 i = 0; i < m_BufferLength; i++)
            {
                quint16 pos = loopBuffer(start + i);
                v_ForFFT[i] = ((PCA_RAW_RGB(pos,0) - mean0)*PCA_Basis(0,0) + (PCA_RAW_RGB(pos,1) - mean1)*PCA_Basis(1,0) + (PCA_RAW_RGB(pos,2) - mean2)*PCA_Basis(2,0)) / temp_sko;
                buffer_duration += v_Time[loop(temp_position - (m_BufferLength - 1) + i)];
            }
        }
        emit PCAProjectionUpdated(v_ForFFT, m_BufferLength);
    }
    else
    {
        for (unsigned int i = 0; i < m_BufferLength; i++)
        {
            quint16 pos = loop(temp_position - (m_BufferLength - 1) + i);
            v_ForFFT[i] = v_Signal[pos];
            buffer_duration += v_Time[pos];
        }
    }

    fftw_execute(m_plan); // Datas were prepared, now execute fftw_plan

    for (quint16 i = 0; i < (m_BufferLength/2 + 1); i++)
    {
        v_Amplitude[i] = v_Spectrum[i][0]*v_Spectrum[i][0] + v_Spectrum[i][1]*v_Spectrum[i][1];
    }
    emit SpectrumUpdated(v_Amplitude, m_BufferLength/2 + 1);

    quint16 bottom_bound = (quint16)(BOTTOM_LIMIT * buffer_duration / 1000.0);   // You should ensure that ( LOW_HR_LIMIT < discretization frequency / 2 )
    quint16 top_bound = (quint16)(TOP_LIMIT * buffer_duration / 1000.0);
    if(top_bound > (m_BufferLength / 2 + 1))
    {
        top_bound = m_BufferLength / 2 + 1;
    }
    quint16 index_of_maxpower = 0;
    qreal maxpower = 0.0;
    for (quint16 i = ( bottom_bound + HALF_INTERVAL ); i < ( top_bound - HALF_INTERVAL ); i++)
    {
        qreal temp_power = v_Amplitude[i];
        if ( maxpower < temp_power )
        {
            maxpower = temp_power;
            index_of_maxpower = i;
        }
    }
    /*-------------------------SNR estimation evaluation-----------------------*/
    qreal noise_power = 0.0;
    qreal signal_power = 0.0;
    for (quint16 i = bottom_bound; i < top_bound; i++)
    {
        if ( (i >= (index_of_maxpower - HALF_INTERVAL )) && (i <= (index_of_maxpower + HALF_INTERVAL)) )
        {
            signal_power += v_Amplitude[i];
        }
        else
        {
            noise_power += v_Amplitude[i];
        }
    }
    m_SNR = 10 * log10( signal_power / noise_power );

    qreal power_multiplyed_by_index = 0.0;
    qreal power_of_first_harmonic = 0.0;
    for (qint16 i = (index_of_maxpower - HALF_INTERVAL); i <= (index_of_maxpower + HALF_INTERVAL); i++)
    {
        power_of_first_harmonic += v_Amplitude[i];
        power_multiplyed_by_index += i * v_Amplitude[i];
    }
    qreal bias = (qreal)index_of_maxpower - ( power_multiplyed_by_index / power_of_first_harmonic );
    bias = sqrt(bias * bias); // take abs of bias
    qreal resultWeight = (HALF_INTERVAL + 1 - bias)/(HALF_INTERVAL + 1);
    m_SNR *= resultWeight * resultWeight * resultWeight * resultWeight; // make more multiplication to add more nonlinearity
    emit snrUpdated(m_ID, m_SNR); // signal for mapper

    if(m_SNR > SNR_TRESHOLD)
    {
        m_HeartRate = (power_multiplyed_by_index / power_of_first_harmonic) * 60000.0 / buffer_duration;
        if((m_HeartRate <= m_rightTreshold) && (m_HeartRate >= m_leftThreshold))
            emit HeartRateUpdated(m_HeartRate, m_SNR, true);
        else
            emit HeartRateUpdated(m_HeartRate, m_SNR, false);
    }
    else
       emit TooNoisy(m_SNR);
}

//----------------------------------------------------------------------------------------------------

void QHarmonicProcessor::setPCAMode(bool value)
{
    f_PCA = value;
}

//----------------------------------------------------------------------------------------------------

void QHarmonicProcessor::switchColorMode(ColorChannel value)
{
    m_channel = value;
}

//----------------------------------------------------------------------------------------------------

void QHarmonicProcessor::CountFrequency()
{
    qint16 position = curpos - 1; // delay on 1 count is critical valuable here
    quint16 watchDogCounter = 0;
    quint16 sign_changes = m_PulseCounter;
    qreal temp_time = 0.0;

    while((v_BinaryOutput[loop(position)]*v_BinaryOutput[loop(position-1)] > 0.0) && (watchDogCounter < m_DataLength))
    {
        position--;
        watchDogCounter++;
    }

    while((sign_changes > 0) && (watchDogCounter < m_DataLength))
    {
        if(v_BinaryOutput[loop(position)]*v_BinaryOutput[loop(position-1)] < 0.0)
        {
            sign_changes--;
        }
        position--;
        watchDogCounter++;
        temp_time += v_Time[loop(position)];
    }

    m_HeartRate = 60.0 * (m_PulseCounter - 1) / ((temp_time - v_Time[loop(position)])/1000.0);
    emit HeartRateUpdated(m_HeartRate,0.0,true);
}

//----------------------------------------------------------------------------------------------------

int QHarmonicProcessor::loadWarningRates(const char *fileName, SexID sex, int age, TwoSideAlpha alpha)
{
    if( !QFile::exists( fileName ) ) {
        return FileExistanceError;
    }

    QFile file(fileName);
    if ( !file.open(QIODevice::ReadOnly) ) {
        return FileOpenError;
    }

    QXmlStreamReader reader(&file);
    QString desiredSex;
    switch(sex)   {
        case Male:
            desiredSex = "male";
            break;
        default:
            desiredSex = "female";
            break;
    }
    QString lowerPercentile;
    QString highestPercentile;
    switch(alpha) {
        case TwoPercents:
            lowerPercentile = "percentile1.0";
            highestPercentile = "percentile99.0";
            break;
        case FivePercents:
            lowerPercentile = "percentile2.5";
            highestPercentile = "percentile97.5";
            break;
        case TenPercents:
            lowerPercentile = "percentile5.0";
            highestPercentile = "percentile95.0";
            break;
        case TwentyPercents:
            lowerPercentile = "percentile10.0";
            highestPercentile = "percentile90.0";
            break;
        default:
            lowerPercentile = "percentile25.0";
            highestPercentile = "percentile75.0";
            break;
    }

    bool FoundSexSection = false;
    bool FoundAgeSection = false;
    bool FoundLowerPercentile = false;
    bool FoundHighestPercentile = false;
    bool ConversionResult1 = false;
    bool ConversionResult2 = false;
    qreal tempLeft = 0.0;
    qreal tempRight = 0.0;

    while(!reader.atEnd())   { // read to the end of xml file
        reader.readNext();
        if(reader.error()) {
            return ParseFailure;
        }
        else {
            if(reader.attributes().hasAttribute("type")) {
                if(reader.attributes().value("type") == desiredSex)
                    FoundSexSection = true;
            }
            if(FoundSexSection && reader.attributes().hasAttribute("agefrom")) {
                if( (age >= reader.attributes().value("agefrom").toInt()) && (age <= reader.attributes().value("ageto").toInt()))
                    FoundAgeSection = true;
            }
            if(FoundSexSection && FoundAgeSection) {
                if(reader.isStartElement() && (reader.name() == lowerPercentile))
                    FoundLowerPercentile = true;
                if(reader.isStartElement() && (reader.name() == highestPercentile))
                    FoundHighestPercentile = true;
            }
            if(FoundLowerPercentile && reader.isCharacters()) {
                tempLeft = reader.text().toDouble(&ConversionResult1);
                qWarning("leftTreshold: %f", tempLeft);
            }
            if(FoundLowerPercentile && reader.isEndElement() && (reader.name() == lowerPercentile))
                FoundLowerPercentile = false;

            if(FoundHighestPercentile && reader.isCharacters()) {
                tempRight = reader.text().toDouble(&ConversionResult2);
                qWarning("RightTreshold: %f", tempRight);
            }
            if(ConversionResult1 && ConversionResult2) {
                m_leftThreshold = tempLeft;
                m_rightTreshold = tempRight;
                return NoError;
            }
        }
    }
    qWarning("Xml parsing:can not find appropriate record in file!");
    return ReadError;
}

//------------------------------------------------------------------------------------------------

void QHarmonicProcessor::setID(quint32 value)
{
    m_ID = value;
}

//------------------------------------------------------------------------------------------------

QHarmonicProcessorMap::QHarmonicProcessorMap(QObject *parent, quint32 width, quint32 height):
    QObject(parent),
    m_width(width),
    m_height(height),
    m_length(width*height),
    m_updations(0),
    m_min(0.0),
    m_max(0.0),
    m_cell(0)
{
    v_map = new qreal[width*height]; // 0...width*height-1
    v_processors = new QHarmonicProcessor[width*height]; // 0...width*height-1
    v_threads = new QThread[width]; // 0..width-1, one thred per column of the map

    for(quint32 i = 0; i < width*height; i++)
    {
        v_processors[i].setID(i); // needs for control in whitch cell of the map write particular snr value
    }

    for(quint16 i = 0; i < height; i++)
    {
        for(quint16 j = 0; j < width; j++)
        {
            v_processors[i*width+j].moveToThread(&v_threads[j]);
            connect(this, &QHarmonicProcessorMap::updateMap, &v_processors[i*width+j], &QHarmonicProcessor::ComputeFrequency);
            connect(&v_processors[i*width+j], &QHarmonicProcessor::snrUpdated, this, &QHarmonicProcessorMap::updateCell);
        }
    }

    for(quint16 i = 0; i < width ; i++)
    {
        v_threads[i].start();
        qWarning("Thread %d was started!", i);
    }
}

QHarmonicProcessorMap::~QHarmonicProcessorMap()
{
    for(quint16 i = 0; i < m_width ; i++)
    {
        v_threads[i].quit();
    }
    for(quint16 i = 0; i < m_width ; i++)
    {
        v_threads[i].wait();
    }
    delete[] v_map;
    delete[] v_processors;
    delete[] v_threads;
}

void QHarmonicProcessorMap::updateHarmonicProcessor(unsigned long red, unsigned long green, unsigned long blue, unsigned long area, double period)
{
    connect(this, &QHarmonicProcessorMap::dataArrived, &v_processors[m_cell], &QHarmonicProcessor::EnrollData);
    emit dataArrived(red,green,blue,area,period);
    disconnect(this, &QHarmonicProcessorMap::dataArrived, &v_processors[m_cell], &QHarmonicProcessor::EnrollData);
    m_cell = (++m_cell) % m_length;
}

void QHarmonicProcessorMap::updateCell(quint32 id, qreal value)
{
    m_mutex.lock();

    v_map[id] = value;
    if(value > m_max) {
        m_max = value;
    } else if(value < m_min) {
        m_min = value;
    }
    m_updations++;
    if(m_updations == m_length)
    {
        for(quint32 i = 0; i < m_length; i++) {
            v_map[i] = (v_map[i] - m_min)/(m_max - m_min);
        }
        emit mapUpdated(v_map, m_width, m_height, m_max, m_min);
        m_updations = 0;
        m_max = 0.0;
        m_min = 0.0;
    }

    m_mutex.unlock();
}


