#pragma once

#include "gui/screens/auth/AuthFormScreen.hpp"
#include "gui/widgets/PolicyReader.hpp"
#include "platform/AsyncTask.hpp"

#include <optional>
#include <vector>

class RegisterScreen : public AuthFormScreen {
public:
    explicit RegisterScreen(AppContext& app);

    void update(const InputFrame& input) override;
    void render() override;

private:
    using Policy = std::optional<std::vector<PolicyBlock>>;

    void submit() override;
    void openConsentDocument() override;

    AsyncTask<Policy> policyTask_;
    PolicyReader reader_;
};
