/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/slave/logging.hpp>

#include <cassert>
#include <cerrno>
#include <ios>
#include <stdexcept>

#include <coral/error.hpp>
#include <coral/log.hpp>
#include <coral/util.hpp>


namespace coral
{
namespace slave
{


LoggingInstance::LoggingInstance(
    std::shared_ptr<Instance> instance,
    const std::string& outputFilePrefix)
    : m_instance{instance}
    , m_outputFilePrefix(outputFilePrefix)
{
    if (m_outputFilePrefix.empty()) m_outputFilePrefix = "./";
}


coral::model::SlaveTypeDescription LoggingInstance::TypeDescription() const
{
    return m_instance->TypeDescription();
}


void LoggingInstance::Setup(
    const std::string& slaveName,
    const std::string& executionName,
    coral::model::TimePoint startTime,
    coral::model::TimePoint stopTime,
    bool adaptiveStepSize,
    double relativeTolerance)
{
    m_instance->Setup(
        slaveName, executionName,
        startTime, stopTime,
        adaptiveStepSize, relativeTolerance);

    auto outputFileName = m_outputFilePrefix;
    if (executionName.empty()) {
        outputFileName += coral::util::Timestamp();
    } else {
        outputFileName += executionName;
    }
    outputFileName += '_';
    if (slaveName.empty()) {
        outputFileName += TypeDescription().Name() + '_'
            + coral::util::RandomString(6, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    } else {
        outputFileName += slaveName;
    }
    outputFileName += ".csv";

    CORAL_LOG_TRACE("LoggingInstance: Opening " + outputFileName);
    m_outputStream.open(
        outputFileName,
        std::ios_base::out | std::ios_base::trunc
#ifdef _MSC_VER
        , _SH_DENYWR // Don't let other processes/threads write to the file
#endif
        );
    if (!m_outputStream.is_open()) {
        const int e = errno;
        throw std::runtime_error(coral::error::ErrnoMessage(
            "Error opening file \"" + outputFileName + "\" for writing",
            e));
    }

    m_outputStream << "Time";
    const auto typeDescription  = TypeDescription();
    for (const auto& var : typeDescription.Variables()) {
        m_outputStream << "," << var.Name();
    }
    m_outputStream << std::endl;
}


void LoggingInstance::StartSimulation()
{
    m_instance->StartSimulation();
}


void LoggingInstance::EndSimulation()
{
    m_instance->EndSimulation();
}


namespace
{
    void PrintVariable(
        std::ostream& out,
        const coral::model::VariableDescription& varInfo,
        Instance& slaveInstance)
    {
        out << ",";
        const auto varID = varInfo.ID();
        switch (varInfo.DataType()) {
            case coral::model::REAL_DATATYPE:
                {
                    double val = 0.0;
                    slaveInstance.GetRealVariables(gsl::make_span(&varID, 1), gsl::make_span(&val, 1));
                    out << val;
                }
                break;
            case coral::model::INTEGER_DATATYPE:
                {
                    int val = 0;
                    slaveInstance.GetIntegerVariables(gsl::make_span(&varID, 1), gsl::make_span(&val, 1));
                    out << val;
                }
                break;
            case coral::model::BOOLEAN_DATATYPE:
                {
                    bool val = false;
                    slaveInstance.GetBooleanVariables(gsl::make_span(&varID, 1), gsl::make_span(&val, 1));
                    out << val;
                }
                break;
            case coral::model::STRING_DATATYPE:
                {
                    std::string val;
                    slaveInstance.GetStringVariables(gsl::make_span(&varID, 1), gsl::make_span(&val, 1));
                    out << val;
                }
                break;
            default:
                assert (false);
        }
    }
}


bool LoggingInstance::DoStep(
    coral::model::TimePoint currentT,
    coral::model::TimeDuration deltaT)
{
    const auto ret = m_instance->DoStep(currentT, deltaT);

    m_outputStream << (currentT + deltaT);
    const auto typeDescription = TypeDescription();
    for (const auto& var : typeDescription.Variables()) {
        PrintVariable(m_outputStream, var, *this);
    }
    m_outputStream << std::endl;

    return ret;
}


void LoggingInstance::GetRealVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<double> values) const
{
    m_instance->GetRealVariables(variables, values);
}


void LoggingInstance::GetIntegerVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<int> values) const
{
    m_instance->GetIntegerVariables(variables, values);
}


void LoggingInstance::GetBooleanVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<bool> values) const
{
    m_instance->GetBooleanVariables(variables, values);
}


void LoggingInstance::GetStringVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<std::string> values) const
{
    m_instance->GetStringVariables(variables, values);
}


bool LoggingInstance::SetRealVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<const double> values)
{
    return m_instance->SetRealVariables(variables, values);
}


bool LoggingInstance::SetIntegerVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<const int> values)
{
    return m_instance->SetIntegerVariables(variables, values);
}


bool LoggingInstance::SetBooleanVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<const bool> values)
{
    return m_instance->SetBooleanVariables(variables, values);
}


bool LoggingInstance::SetStringVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<const std::string> values)
{
    return m_instance->SetStringVariables(variables, values);
}


}} // namespace
