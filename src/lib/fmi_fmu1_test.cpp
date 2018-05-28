#include <cstdlib>
#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <coral/fmi/importer.hpp>
#include <coral/fmi/fmu1.hpp>
#include <coral/util/filesystem.hpp>
#include <coral/util/zip.hpp>


namespace
{
    void RunTests(std::shared_ptr<coral::fmi::FMU> fmu)
    {
        EXPECT_EQ(coral::fmi::FMIVersion::v1_0, fmu->FMIVersion());
        const auto& d = fmu->Description();
        EXPECT_EQ("no.viproma.demo.identity", d.Name());
        EXPECT_EQ(36U, d.UUID().size());
        EXPECT_EQ(
            "Has one input and one output of each type, and outputs are always set equal to inputs",
            d.Description());
        EXPECT_EQ("Lars Tandle Kyllingstad", d.Author());
        EXPECT_EQ("0.3", d.Version());
        EXPECT_NE(nullptr, std::static_pointer_cast<coral::fmi::FMU1>(fmu)->FmilibHandle());

        coral::model::VariableID
            realIn  = 0, integerIn  = 0, booleanIn  = 0, stringIn = 0,
            realOut = 0, integerOut = 0, booleanOut = 0, stringOut = 0;
        for (const auto& v : d.Variables()) {
            if      (v.Name() ==    "realIn" )    realIn  = v.ID();
            else if (v.Name() == "integerIn" ) integerIn  = v.ID();
            else if (v.Name() == "booleanIn" ) booleanIn  = v.ID();
            else if (v.Name() ==  "stringIn" )  stringIn  = v.ID();
            else if (v.Name() ==    "realOut")    realOut = v.ID();
            else if (v.Name() == "integerOut") integerOut = v.ID();
            else if (v.Name() == "booleanOut") booleanOut = v.ID();
            else if (v.Name() ==  "stringOut")  stringOut = v.ID();

            if (v.Name() ==    "realIn" ) {
                EXPECT_EQ(coral::model::REAL_DATATYPE, v.DataType());
                EXPECT_EQ(coral::model::DISCRETE_VARIABILITY, v.Variability());
                EXPECT_EQ(coral::model::INPUT_CAUSALITY, v.Causality());
            } else if (v.Name() == "stringOut") {
                EXPECT_EQ(coral::model::STRING_DATATYPE, v.DataType());
                EXPECT_EQ(coral::model::DISCRETE_VARIABILITY, v.Variability());
                EXPECT_EQ(coral::model::OUTPUT_CAUSALITY, v.Causality());
            }
        }

        const double tMax = 1.0;
        const double dt = 0.1;
        double realVal = 0.0;
        int integerVal = 0;
        bool booleanVal = false;
        std::string stringVal;

        auto instance = fmu->InstantiateSlave();
        instance->Setup("testSlave", "testExecution", 0.0, tMax, false, 0.0);
        instance->StartSimulation();

        for (double t = 0; t < tMax; t += dt) {
            double getRealVal = -1.0;
            int getIntegerVal = -1;
            bool getBooleanVal = true;
            std::string getStringVal = "unexpected value";

            instance->GetRealVariables   (gsl::make_span(&realOut,    1), gsl::make_span(&getRealVal, 1));
            instance->GetIntegerVariables(gsl::make_span(&integerOut, 1), gsl::make_span(&getIntegerVal, 1));
            instance->GetBooleanVariables(gsl::make_span(&booleanOut, 1), gsl::make_span(&getBooleanVal, 1));
            instance->GetStringVariables (gsl::make_span(&stringOut,  1), gsl::make_span(&getStringVal, 1));

            EXPECT_EQ(realVal,    getRealVal);
            EXPECT_EQ(integerVal, getIntegerVal);
            EXPECT_EQ(booleanVal, getBooleanVal);
            EXPECT_EQ(stringVal,  getStringVal);

            realVal += 1.0;
            integerVal += 1;
            booleanVal = !booleanVal;
            stringVal += 'a';

            instance->SetRealVariables   (gsl::make_span(&realIn,    1), gsl::make_span(&realVal,    1));
            instance->SetIntegerVariables(gsl::make_span(&integerIn, 1), gsl::make_span(&integerVal, 1));
            instance->SetBooleanVariables(gsl::make_span(&booleanIn, 1), gsl::make_span(&booleanVal, 1));
            instance->SetStringVariables (gsl::make_span(&stringIn,  1), gsl::make_span(&stringVal , 1));

            EXPECT_TRUE(instance->DoStep(t, dt));
        }

        instance->EndSimulation();
    }
}


TEST(coral_fmi, Fmu1)
{
    const auto testDataDir = std::getenv("CORAL_TEST_DATA_DIR");
    auto importer = coral::fmi::Importer::Create();
    auto fmu = importer->Import(
        boost::filesystem::path(testDataDir) / "fmi1_cs" / "identity.fmu");
    RunTests(fmu);
}


TEST(coral_fmi, Fmu1_unpacked)
{
    const auto testDataDir = std::getenv("CORAL_TEST_DATA_DIR");
    coral::util::TempDir unpackDir;
    coral::util::zip::Archive(
            boost::filesystem::path(testDataDir) / "fmi1_cs" / "identity.fmu")
        .ExtractAll(unpackDir.Path());

    auto importer = coral::fmi::Importer::Create();
    auto fmu = importer->ImportUnpacked(unpackDir.Path());
    RunTests(fmu);

    importer->CleanCache();
    EXPECT_TRUE(boost::filesystem::exists(unpackDir.Path()));
}
