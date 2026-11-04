#pragma once

#include "core/AsyncTask.hpp"
#include "gui/elements/PolicyReader.hpp"
#include "gui/screens/AuthFormScreen.hpp"

#include <optional>
#include <vector>

class RegisterScreen : public AuthFormScreen {
public:
    explicit RegisterScreen(App& app);

    void update(const InputFrame& input) override;
    void render() override;

private:
    using Policy = std::optional<std::vector<PolicyBlock>>;

    void submit() override;
    void openConsentDocument() override;

    AsyncTask<Policy> policyTask_;
    PolicyReader reader_;
};
