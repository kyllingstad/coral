/*
Copyright 2013-present, SINTEF Ocean.
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include <coral/fmi/fmu1.hpp>

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <boost/numeric/conversion/cast.hpp>
#include <fmilib.h>

#include <coral/error.hpp>
#include <coral/fmi/glue.hpp>
#include <coral/fmi/importer.hpp>
#include <coral/log.hpp>
#include <coral/util.hpp>

#ifdef _WIN32
#include <coral/fmi/windows.hpp>
#endif


namespace coral
{
namespace fmi
{

// =============================================================================
// FMU1
// =============================================================================

FMU1::FMU1(
    std::shared_ptr<coral::fmi::Importer> importer,
    const boost::filesystem::path& fmuDir)
    : m_importer{importer}
    , m_dir(fmuDir)
    , m_handle{fmi1_import_parse_xml(importer->FmilibHandle(), fmuDir.string().c_str())}
{
    if (m_handle == nullptr) {
        throw std::runtime_error(importer->LastErrorMessage());
    }
    const auto fmuKind = fmi1_import_get_fmu_kind(m_handle);
    if (fmuKind != fmi1_fmu_kind_enu_cs_standalone &&
        fmuKind != fmi1_fmu_kind_enu_cs_tool) {
        throw std::runtime_error("Not a co-simulation FMU");
    }

    // Create the slave type description object
    const auto varList = fmi1_import_get_variable_list(m_handle);
    const auto freeVarList = coral::util::OnScopeExit([&]() {
        fmi1_import_free_variable_list(varList);
    });
    std::vector<coral::model::VariableDescription> variables;
    const auto varCount = fmi1_import_get_variable_list_size(varList);
    for (unsigned int i = 0; i < varCount; ++i) {
        const auto var = fmi1_import_get_variable(varList, i);
        m_valueReferences.push_back(fmi1_import_get_variable_vr(var));
        variables.push_back(
            ToVariable(var, boost::numeric_cast<coral::model::VariableID>(i)));
    }
    m_description = std::make_unique<coral::model::SlaveTypeDescription>(
        std::string(fmi1_import_get_model_name(m_handle)),
        std::string(fmi1_import_get_GUID(m_handle)),
        std::string(fmi1_import_get_description(m_handle)),
        std::string(fmi1_import_get_author(m_handle)),
        std::string(fmi1_import_get_model_version(m_handle)),
        variables);
}


FMU1::~FMU1()
{
    fmi1_import_free(m_handle);
}


coral::fmi::FMIVersion FMU1::FMIVersion() const
{
    return coral::fmi::FMIVersion::v1_0;
}


const coral::model::SlaveTypeDescription& FMU1::Description() const
{
    return *m_description;
}


std::shared_ptr<SlaveInstance> FMU1::InstantiateSlave()
{
    return InstantiateSlave1();
}


std::shared_ptr<coral::fmi::Importer> FMU1::Importer() const
{
    return m_importer;
}


namespace
{
    void Prune(std::vector<std::weak_ptr<SlaveInstance1>>& instances)
    {
        auto newEnd = std::remove_if(
            begin(instances),
            end(instances),
            [] (const std::weak_ptr<SlaveInstance1>& wp) { return wp.expired(); });
        instances.erase(newEnd, end(instances));
    }
}


std::shared_ptr<SlaveInstance1> FMU1::InstantiateSlave1()
{
#ifdef _WIN32
    if (!m_additionalDllSearchPath) {
        m_additionalDllSearchPath =
            std::make_unique<AdditionalPath>(FMUBinariesDir(m_dir));
    }
#endif
    Prune(m_instances);
    const bool isSingleton = !!fmi1_import_get_canBeInstantiatedOnlyOncePerProcess(
        fmi1_import_get_capabilities(m_handle));
    if (isSingleton && !m_instances.empty()) {
        throw std::runtime_error("FMU can only be instantiated once");
    }
    auto instance =
        std::shared_ptr<SlaveInstance1>(new SlaveInstance1(shared_from_this()));
    m_instances.push_back(instance);
    return instance;
}


const boost::filesystem::path& FMU1::Directory() const
{
    return m_dir;
}


fmi1_value_reference_t FMU1::FMIValueReference(coral::model::VariableID variable)
    const
{
    return m_valueReferences.at(variable);
}


fmi1_import_t* FMU1::FmilibHandle() const
{
    return m_handle;
}


// =============================================================================
// SlaveInstance1
// =============================================================================

namespace
{
    void StepFinishedPlaceholder(fmi1_component_t, fmi1_status_t)
    {
        CORAL_LOG_DEBUG("FMU instance completed asynchronous step, "
            "but this feature is currently not supported");
    }

    struct LogRecord
    {
        LogRecord() { }
        LogRecord(fmi1_status_t s, const std::string& m) : status{s}, message(m) { }
        fmi1_status_t status = fmi1_status_ok;
        std::string message;
    };
    std::unordered_map<std::string, LogRecord> g_logRecords;
    std::mutex g_logMutex;

    void LogMessage(
        fmi1_component_t c,
        fmi1_string_t instanceName,
        fmi1_status_t status,
        fmi1_string_t category,
        fmi1_string_t message,
        ...)
    {
        std::va_list args;
        va_start(args, message);
        const auto msgLength = std::vsnprintf(nullptr, 0, message, args);
        va_end(args);
        auto msgBuffer = std::vector<char>(msgLength+1);
        va_start(args, message);
        std::vsnprintf(msgBuffer.data(), msgBuffer.size(), message, args);
        va_end(args);
        assert(msgBuffer.back() == '\0');

        std::string statusName = "UNKNOWN";
        coral::log::Level logLevel = coral::log::error;
        switch (status) {
            case fmi1_status_ok:
                statusName = "OK";
                logLevel = coral::log::info;
                break;
            case fmi1_status_warning:
                statusName = "WARNING";
                logLevel = coral::log::warning;
                break;
            case fmi1_status_discard:
                // Don't know if this ever happens, but we should at least
                // print a debug message if it does.
                statusName = "DISCARD";
                logLevel = coral::log::debug;
                break;
            case fmi1_status_error:
                statusName = "ERROR";
                logLevel = coral::log::error;
                break;
            case fmi1_status_fatal:
                statusName = "FATAL";
                logLevel = coral::log::error;
                break;
            case fmi1_status_pending:
                // Don't know if this ever happens, but we should at least
                // print a debug message if it does.
                statusName = "PENDING";
                logLevel = coral::log::debug;
                break;
        }

        if (logLevel < coral::log::error) {
            // Errors are not logged; we handle them with exceptions instead.
            coral::log::Log(logLevel, msgBuffer.data());
        }

        g_logMutex.lock();
        g_logRecords[instanceName] =
            LogRecord{status, std::string(msgBuffer.data())};
        g_logMutex.unlock();
    }

    LogRecord LastLogRecord(const std::string& instanceName)
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        const auto it = g_logRecords.find(instanceName);
        if (it == g_logRecords.end()) {
            return LogRecord{};
        } else {
            // Note the use of c_str() here, to force the string to be copied.
            // The C++ standard now disallows copy-on-write, but some compilers
            // still use it, which could lead to problems in multithreaded
            // programs.
            return LogRecord{
                it->second.status,
                std::string(it->second.message.c_str())
            };
        }
    }
}


SlaveInstance1::SlaveInstance1(std::shared_ptr<coral::fmi::FMU1> fmu)
    : m_fmu{fmu}
    , m_handle{fmi1_import_parse_xml(fmu->Importer()->FmilibHandle(), fmu->Directory().string().c_str())}
{
    if (m_handle == nullptr) {
        throw std::runtime_error(fmu->Importer()->LastErrorMessage());
    }

    fmi1_callback_functions_t callbacks;
    callbacks.allocateMemory = std::calloc;
    callbacks.freeMemory     = std::free;
    callbacks.logger         = LogMessage;
    callbacks.stepFinished   = StepFinishedPlaceholder;

    if (fmi1_import_create_dllfmu(m_handle, callbacks, false) != jm_status_success) {
        const auto msg = fmu->Importer()->LastErrorMessage();
        fmi1_import_free(m_handle);
        throw std::runtime_error(msg);
    }
}


SlaveInstance1::~SlaveInstance1() noexcept
{
    if (m_setupComplete) {
        if (m_simStarted) {
            fmi1_import_terminate_slave(m_handle);
        }
        fmi1_import_free_slave_instance(m_handle);
    }
    fmi1_import_destroy_dllfmu(m_handle);
    fmi1_import_free(m_handle);
}


coral::model::SlaveTypeDescription SlaveInstance1::TypeDescription() const
{
    return FMU()->Description();
}


void SlaveInstance1::Setup(
    const std::string& slaveName,
    const std::string& /*executionName*/,
    coral::model::TimePoint startTime,
    coral::model::TimePoint stopTime,
    bool /*adaptiveStepSize*/,
    double /*relativeTolerance*/)
{
    assert(!m_setupComplete);
    const auto rc = fmi1_import_instantiate_slave(
        m_handle,
        slaveName.c_str(),
        nullptr,
        nullptr,
        0,
        fmi1_false,
        fmi1_false);
    if (rc != jm_status_success) {
        throw std::runtime_error(
            "FMI error: Slave instantiation failed ("
            + LastLogRecord(slaveName).message + ')');
    }
    m_setupComplete = true;

    m_instanceName = slaveName;
    m_startTime = startTime;
    m_stopTime = stopTime;
}


void SlaveInstance1::StartSimulation()
{
    assert(m_setupComplete);
    assert(!m_simStarted);
    const auto rc = fmi1_import_initialize_slave(
        m_handle,
        m_startTime,
        m_stopTime != coral::model::ETERNITY,
        m_stopTime);
    if (rc != fmi1_status_ok && rc != fmi1_status_warning) {
        throw std::runtime_error(
            "FMI error: Failed to initialize slave ("
            + LastLogRecord(m_instanceName).message + ")");
    }
    m_simStarted = true;
}


void SlaveInstance1::EndSimulation()
{
    assert(m_simStarted);
    const auto rc = fmi1_import_terminate_slave(m_handle);
    m_simStarted = false;
    if (rc != fmi1_status_ok && rc != fmi1_status_warning) {
        throw std::runtime_error(
            "FMI error: Failed to terminate slave ("
            + LastLogRecord(m_instanceName).message + ")");
    }
}


bool SlaveInstance1::DoStep(
    coral::model::TimePoint currentT,
    coral::model::TimeDuration deltaT)
{
    assert(m_simStarted);
    const auto rc = fmi1_import_do_step(m_handle, currentT, deltaT, true);
    if (rc == fmi1_status_ok || rc == fmi1_status_warning) {
        return true;
    } else if (rc == fmi1_status_discard) {
        return false;
    } else {
        throw std::runtime_error(
            "Failed to perform time step ("
            + LastLogRecord(m_instanceName).message + ")");
    }
}


namespace
{
    // This is only temporary, until we start using FMI value refs as
    // variable IDs.
    std::vector<fmi1_value_reference_t> FMIValueReferences(
        gsl::span<const coral::model::VariableID> id,
        const FMU1& fmu)
    {
        std::vector<fmi1_value_reference_t> vr(id.size());
        for (int i = 0; i < id.size(); ++i) {
            vr[i] = fmu.FMIValueReference(id[i]);
        }
        return vr;
    }

    std::runtime_error MakeGetOrSetException(
        const std::string& getOrSet,
        const std::string& instanceName)
    {
        return std::runtime_error(
            "Failed to " + getOrSet + " one or more variable values: "
            + LastLogRecord(instanceName).message);
    }
}


void SlaveInstance1::GetRealVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<double> values) const
{
    CORAL_INPUT_CHECK(variables.size() == values.size());
    const auto valRefs = FMIValueReferences(variables, *m_fmu);
    static_assert (std::is_same<double, fmi1_real_t>::value, "type size mismatch");
    const auto status = fmi1_import_get_real(
        m_handle, valRefs.data(), valRefs.size(), values.data());
    if (status != fmi1_status_ok && status != fmi1_status_warning) {
        throw MakeGetOrSetException("get", m_instanceName);
    }
}


void SlaveInstance1::GetIntegerVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<int> values) const
{
    CORAL_INPUT_CHECK(variables.size() == values.size());
    const auto valRefs = FMIValueReferences(variables, *m_fmu);
    static_assert (std::is_same<int, fmi1_integer_t>::value, "type size mismatch");
    const auto status = fmi1_import_get_integer(
        m_handle, valRefs.data(), valRefs.size(), values.data());
    if (status != fmi1_status_ok && status != fmi1_status_warning) {
        throw MakeGetOrSetException("get", m_instanceName);
    }
}


void SlaveInstance1::GetBooleanVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<bool> values) const
{
    CORAL_INPUT_CHECK(variables.size() == values.size());
    const auto valRefs = FMIValueReferences(variables, *m_fmu);

    std::vector<fmi1_boolean_t> fmiValues(values.size());
    const auto status = fmi1_import_get_boolean(
        m_handle, valRefs.data(), valRefs.size(), fmiValues.data());
    if (status != fmi1_status_ok && status != fmi1_status_warning) {
        throw MakeGetOrSetException("get", m_instanceName);
    }
    for (int i = 0; i < values.size(); ++i) {
        values[i] = (fmiValues[i] != 0);
    }
}


void SlaveInstance1::GetStringVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<std::string> values) const
{
    CORAL_INPUT_CHECK(variables.size() == values.size());
    const auto valRefs = FMIValueReferences(variables, *m_fmu);

    std::vector<fmi1_string_t> fmiValues(values.size());
    const auto status = fmi1_import_get_string(
        m_handle, valRefs.data(), valRefs.size(), fmiValues.data());
    if (status != fmi1_status_ok && status != fmi1_status_warning) {
        throw MakeGetOrSetException("get", m_instanceName);
    }
    for (int i = 0; i < values.size(); ++i) {
        values[i] = (fmiValues[i] == nullptr) ? std::string()
                                              : std::string(fmiValues[i]);
    }
}


bool SlaveInstance1::SetRealVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<const double> values)
{
    CORAL_INPUT_CHECK(variables.size() == values.size());
    const auto valRefs = FMIValueReferences(variables, *m_fmu);
    static_assert (std::is_same<double, fmi1_real_t>::value, "type size mismatch");
    const auto status = fmi1_import_set_real(
        m_handle, valRefs.data(), valRefs.size(), values.data());
    if (status == fmi1_status_ok || status == fmi1_status_warning) {
        return true;
    } else if (status == fmi1_status_discard) {
        return false;
    } else {
        throw MakeGetOrSetException("set", m_instanceName);
    }
}


bool SlaveInstance1::SetIntegerVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<const int> values)
{
    CORAL_INPUT_CHECK(variables.size() == values.size());
    const auto valRefs = FMIValueReferences(variables, *m_fmu);
    static_assert (std::is_same<int, fmi1_integer_t>::value, "type size mismatch");
    const auto status = fmi1_import_set_integer(
        m_handle, valRefs.data(), valRefs.size(), values.data());
    if (status == fmi1_status_ok || status == fmi1_status_warning) {
        return true;
    } else if (status == fmi1_status_discard) {
        return false;
    } else {
        throw MakeGetOrSetException("set", m_instanceName);
    }
}


bool SlaveInstance1::SetBooleanVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<const bool> values)
{
    CORAL_INPUT_CHECK(variables.size() == values.size());
    const auto valRefs = FMIValueReferences(variables, *m_fmu);
    std::vector<fmi1_boolean_t> fmiValues(values.size());
    for (int i = 0; i < values.size(); ++i) {
        fmiValues[i] = values[i] ? fmi1_true : fmi1_false;
    }
    const auto status = fmi1_import_set_boolean(
        m_handle, valRefs.data(), valRefs.size(), fmiValues.data());
    if (status == fmi1_status_ok || status == fmi1_status_warning) {
        return true;
    } else if (status == fmi1_status_discard) {
        return false;
    } else {
        throw MakeGetOrSetException("set", m_instanceName);
    }
}


bool SlaveInstance1::SetStringVariables(
    gsl::span<const coral::model::VariableID> variables,
    gsl::span<const std::string> values)
{
    CORAL_INPUT_CHECK(variables.size() == values.size());
    const auto valRefs = FMIValueReferences(variables, *m_fmu);
    std::vector<fmi1_string_t> fmiValues(values.size());
    for (int i = 0; i < values.size(); ++i) {
        fmiValues[i] = values[i].c_str();
    }
    const auto status = fmi1_import_set_string(
        m_handle, valRefs.data(), valRefs.size(), fmiValues.data());
    if (status == fmi1_status_ok || status == fmi1_status_warning) {
        return true;
    } else if (status == fmi1_status_discard) {
        return false;
    } else {
        throw MakeGetOrSetException("set", m_instanceName);
    }
}


std::shared_ptr<coral::fmi::FMU> SlaveInstance1::FMU() const
{
    return FMU1();
}


std::shared_ptr<coral::fmi::FMU1> SlaveInstance1::FMU1() const
{
    return m_fmu;
}


fmi1_import_t* SlaveInstance1::FmilibHandle() const
{
    return m_handle;
}


}} //namespace
